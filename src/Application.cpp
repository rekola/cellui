#include <cellui/Application.h>

#include <cellui/BackendChoice.h>
#include <cellui/KeyStrokeNames.h>
#ifdef CELLUI_HAS_GTK
#include <cellui/gtk/Window.h>
#endif
#ifdef CELLUI_HAS_WIN32
#include <cellui/win32/Window.h>
#endif
#ifndef _WIN32
#include <cellui/term/TerminalSession.h>
#endif

#include <algorithm>
#include <cstring>
#include <string_view>

namespace cellui {

// The display's charts behind one stable object: widgets keep a reference to
// it, and it follows the display when its pictures, cell size or scale change.
class Application::DisplayCharts : public Charts {
 public:
  void reset(const DisplayInfo & display) { inner_ = chartsFor(display); }

  std::shared_ptr<HeatmapChart> heatmap(Plane & parent, int grid_cols, int grid_rows) const override {
    return inner_->heatmap(parent, grid_cols, grid_rows);
  }
  void drawSparkline(Plane & plane, int row, int col, int rows, const std::vector<SparklineReading> & readings, const SparklineLook & look,
                     int clip_top, int clip_bottom, uint64_t id) const override {
    inner_->drawSparkline(plane, row, col, rows, readings, look, clip_top, clip_bottom, id);
  }
  void drawTrace(Plane & plane, int row, int col, int rows, int cols, std::span<const float> values, const TraceLook & look, int clip_top,
                 int clip_bottom, uint64_t id) const override {
    inner_->drawTrace(plane, row, col, rows, cols, values, look, clip_top, clip_bottom, id);
  }
  void drawWaveformRow(Plane & plane, int row, int col, int width, const std::vector<float> & slices, const WaveformLook & look) const override {
    inner_->drawWaveformRow(plane, row, col, width, slices, look);
  }

 private:
  std::unique_ptr<Charts> inner_;
};

namespace {

// The backend of `kind`; `light` is whether its display is light.
std::unique_ptr<Backend> openOne(BackendKind kind, const ApplicationOptions & options, bool & light, std::string * error) {
  switch (kind) {
    case BackendKind::WINDOW: {
#if defined(CELLUI_HAS_GTK)
      auto window = gtk::Window::open({}, error);
#elif defined(CELLUI_HAS_WIN32)
      auto window = win32::Window::open({}, error);
#else
      if (error) *error = "this build has no window backend";
      return nullptr;
#endif
#if defined(CELLUI_HAS_GTK) || defined(CELLUI_HAS_WIN32)
      if (window) {
        if (auto bg = window->display().default_bg) light = bg->luminance() > 0.2f;
      }
      return window;
#endif
    }
    case BackendKind::TERMINAL: {
#ifndef _WIN32
      term::SessionOptions session_options;
      if (options.dither) session_options.dither = *options.dither;
      auto session = term::TerminalSession::open(session_options, error);
      if (session) light = session->capabilities().appearance == term::Appearance::LIGHT;
      return session;
#else
      (void)options;
      if (error) *error = "there is no terminal backend on Windows";
      return nullptr;
#endif
    }
    case BackendKind::AUTO: break;
  }
  if (error) *error = "no backend";
  return nullptr;
}

}  // namespace

std::unique_ptr<Application> Application::open(int argc, char ** argv, std::string * error) {
  ApplicationOptions options;
  std::vector<std::string> rest;
  for (int i = 1; i < argc; i++) {
    std::string_view arg = argv[i];
    std::string_view name;
    bool is_ui = false;
    if (arg == "--ui") {
      is_ui = true;
      if (i + 1 >= argc) {
        if (error) *error = "--ui needs a backend: auto, terminal or window";
        return nullptr;
      }
      name = argv[++i];
    } else if (arg.substr(0, 5) == "--ui=") {
      is_ui = true;
      name = arg.substr(5);
    }
    if (!is_ui && (arg == "--theme" || arg.substr(0, 8) == "--theme=")) {
      if (arg == "--theme") {
        if (i + 1 >= argc) {
          if (error) *error = "--theme needs a name: original, dark or light";
          return nullptr;
        }
        options.theme = argv[++i];
      } else {
        options.theme = std::string(arg.substr(8));
      }
      Theme probe;
      if (!Theme::byName(options.theme, probe)) {
        if (error) *error = "unknown --theme '" + options.theme + "': use original, dark or light";
        return nullptr;
      }
      continue;
    }
    if (!is_ui) {
      rest.emplace_back(arg);
      continue;
    }
    if (!parseBackendKind(name, options.ui)) {
      if (error) *error = "unknown --ui '" + std::string(name) + "': use auto, terminal or window";
      return nullptr;
    }
  }
  auto app = open(std::move(options), error);
  if (app) app->arguments_ = std::move(rest);
  return app;
}

std::unique_ptr<Application> Application::open(ApplicationOptions options, std::string * error) {
  std::string failure;
  for (BackendKind kind : backendCandidates(options.ui, probeBackendEnvironment())) {
    std::string why;
    bool light = false;
    if (auto backend = openOne(kind, options, light, &why)) {
      return std::unique_ptr<Application>(new Application(std::move(backend), {}, std::move(options), kind, light));
    }
    failure += (failure.empty() ? "" : "; ") + std::string(backendName(kind)) + ": " + why;
  }
  if (error) *error = failure;
  return nullptr;
}

std::unique_ptr<Application> Application::adopt(std::unique_ptr<Backend> backend, ApplicationOptions options) {
  return std::unique_ptr<Application>(new Application(std::move(backend), {}, std::move(options), BackendKind::TERMINAL, false));
}

Application::Application(std::unique_ptr<Backend> backend, std::vector<std::string> arguments, ApplicationOptions options, BackendKind kind, bool light)
    : backend_(std::move(backend)),
      arguments_(std::move(arguments)),
      own_theme_(!options.styles),
      styles_(options.styles ? std::move(options.styles) : std::make_unique<StyleProvider>()),
      kind_(kind),
      light_(light),
      buffer_(backend_->rows(), backend_->cols()),
      root_plane_(buffer_, *styles_, std::move(options.app_context)),
      charts_(std::make_unique<DisplayCharts>()),
      dispatcher_(std::make_unique<InputDispatcher>(own_root_widget_)),
      keymap_override_(options.keymap) {
  if (own_theme_) applyTheme(options.theme);
  configurePlane();
  dispatcher_->setDragDropEnabled(backend_->display().drag_drop);
  // A bare Escape waits for the key after it, in case it was an Alt prefix,
  // unless what has the focus wants it at once, or the palette or a modal dialog is up.
  key_reader_.takeEscape = [this](const Input & escape) {
    Widget * focused = dispatcher_->focused();
    if (!palette_.isOpen() && !own_root_widget_.activeModal() && (!focused || !focused->wantsBareEscape())) return false;
    dispatcher_->offer(escape);
    return true;
  };
  commands_.define("command-palette", "Search the commands and run one", [this] { openCommandPalette(); });
  keymap_.bind(KeymapProfile::TERMINAL, "M-x", "command-palette");
  keymap_.bind(KeymapProfile::DESKTOP, "Ctrl+Shift+P", "command-palette");
  installRootHandlers();
}

// On the root of the widget tree: the palette ahead of everything while it is
// open, then the key after a prefix (capture, ahead of the focused widget),
// and the keymap for a key the focused widget left (bubble), unless the
// program offers keys to it itself (KeymapOrder::PROGRAM).
void Application::installRootHandlers() {
  root_handlers_.clear();
  Widget & root = *root_widget_;
  auto yes = [](bool taken) { return taken ? Handled::yes : Handled::no; };
  root_handlers_.push_back(root.on<KeyDown>([this, yes](const KeyDown & e, EventContext &) {
    if (palette_.isOpen()) {
      paletteResult(palette_.key(e));
      return Handled::yes;
    }
    return yes(prefixedKey(e));
  },
                                            Phase::capture));
  root_handlers_.push_back(root.on<KeyUp>([this, yes](const KeyUp &, EventContext &) { return yes(palette_.isOpen()); }, Phase::capture));
  root_handlers_.push_back(root.on<MouseDown>([this](const MouseDown & e, EventContext &) {
    if (!palette_.isOpen()) return Handled::no;
    if (e.button == 1) paletteResult(palette_.press(e.row, e.col));
    return Handled::yes;
  },
                                              Phase::capture));
  root_handlers_.push_back(root.on<MouseMove>([this](const MouseMove & e, EventContext &) {
    if (!palette_.isOpen()) return Handled::no;
    if (e.buttons & 1u) paletteResult(palette_.drag(e.row, e.col));
    return Handled::yes;
  },
                                              Phase::capture));
  root_handlers_.push_back(root.on<MouseUp>([this](const MouseUp & e, EventContext &) {
    if (!palette_.isOpen()) return Handled::no;
    if (e.button == 1) paletteResult(palette_.click(e.row, e.col));
    return Handled::yes;
  },
                                            Phase::capture));
  root_handlers_.push_back(root.on<Wheel>([this](const Wheel & e, EventContext &) {
    if (!palette_.isOpen()) return Handled::no;
    if (e.dy != 0) paletteResult(palette_.wheel(e.dy > 0));
    return Handled::yes;
  },
                                          Phase::capture));
  root_handlers_.push_back(root.on<KeyDown>([this, yes](const KeyDown & e, EventContext &) {
    return yes(keymap_order_ == KeymapOrder::AFTER_FOCUS && keyBinding(e));
  }));
}

namespace {

// The stroke a key is bound as. On the terminal keymap Meta is Alt (M-x), however the terminal sent it.
uint64_t strokeOf(const KeyDown & e, KeymapProfile profile) {
  Modifiers mods = e.mods;
  if (profile == KeymapProfile::TERMINAL && mods.meta) {
    mods.alt = true;
    mods.meta = false;
  }
  return KeyStroke::pack(e.key, mods);
}

}  // namespace

bool Application::prefixedKey(const KeyDown & e) {
  if (!pending_prefix_) return false;
  uint64_t prefix = *pending_prefix_;
  pending_prefix_.reset();
  uint64_t stroke = strokeOf(e, profile());
  // A started sequence takes the key after it, bound or not, so a half-typed prefix never does something else.
  if (const std::string * name = keymap_.lookupPrefixed(prefix, stroke, profile())) {
    message.emit("");
    runBinding(*name);
  } else {
    message.emit(KeyStroke::describe({prefix, stroke}, profile()) + " is undefined");
  }
  return true;
}

bool Application::keyBinding(const KeyDown & e) {
  uint64_t stroke = strokeOf(e, profile());
  if (keymap_.isPrefix(stroke, profile())) {
    pending_prefix_ = stroke;
    message.emit(KeyStroke::describe({stroke}, profile()) + "-");
    return true;
  }
  if (const std::string * name = keymap_.lookup(stroke, profile())) return runBinding(*name);
  return false;
}

bool Application::execute(const std::string & name) {
  if (commands_.execute(name)) return true;
  return command_source_.execute && command_source_.execute(name);
}

bool Application::runBinding(const std::string & name) {
  if (commands_.execute(name)) return true;
  if (command_source_.run_binding) return command_source_.run_binding(name);
  return command_source_.execute && command_source_.execute(name);
}

std::string Application::bindingFor(const std::string & command) const {
  std::string binding = command_source_.binding ? command_source_.binding(command) : std::string();
  if (binding.empty()) binding = keymap_.bindingFor(command, root_plane_.keymapProfile());
  return binding;
}

void Application::openCommandPalette() {
  pending_prefix_.reset();
  std::vector<std::string> names = commands_.names();
  if (command_source_.names) {
    for (std::string & name : command_source_.names()) {
      if (!commands_.has(name)) names.push_back(std::move(name));
    }
  }
  std::vector<PaletteEntry> entries;
  for (const std::string & name : names) {
    std::string description = commands_.description(name);
    if (description.empty() && command_source_.description) description = command_source_.description(name);
    entries.push_back({name, std::move(description), bindingFor(name)});
  }
  palette_.open(std::move(entries));
  layoutPalette();
}

void Application::layoutPalette() {
  if (palette_.isOpen()) palette_.show(root_plane_, rows(), cols());
}

void Application::paletteResult(CommandPalette::Result result) {
  if (result == CommandPalette::Result::NONE) {
    layoutPalette();  // the new query or selection
    return;
  }
  std::string name;
  if (result == CommandPalette::Result::CHOSEN && palette_.chosen()) name = palette_.chosen()->name;
  // Closed before the command runs, which may open something of its own.
  palette_.close();
  if (!name.empty() && !execute(name)) message.emit("No command " + name);
}

Application::~Application() = default;

const Charts & Application::charts() const { return *charts_; }

// The display's own colours decide: a terminal of 16 or 8 colours gets a theme
// made of them, any other the modern look, light on a light background.
void Application::applyTheme(const std::string & name) {
  const DisplayInfo & display = backend_->display();
  Theme theme;
  if (!name.empty() && Theme::byName(name, theme)) {
    theme_name_ = name;
  } else if (display.palette_colours == 16) {
    theme = Theme::ansi16(display.default_fg, display.default_bg);
    theme_name_ = "ansi16";
  } else if (display.palette_colours == 8) {
    theme = Theme::ansi16(display.default_fg, display.default_bg, true);
    theme_name_ = "ansi8";
  } else {
    bool light = display.default_bg && display.default_bg->luminance() > 0.2f;
    theme = light ? Theme::light() : Theme::dark();
    theme_name_ = light ? "light" : "dark";
  }
  styles_->applyTheme(theme);
}

bool Application::setTheme(const std::string & name) {
  Theme probe;
  if (!own_theme_ || !Theme::byName(name, probe)) return false;
  applyTheme(name);
  root_plane_.invalidateSharedImages();
  return true;
}

void Application::configurePlane() {
  const DisplayInfo & display = backend_->display();
  if (display.default_fg && display.default_bg) root_plane_.setDefaultColors(*display.default_fg, *display.default_bg);
  root_plane_.setDisplayScale(display.display_scale);
  root_plane_.setHdrPictures(display.hdr_pictures);
  root_plane_.setProportionalText(display.proportional_text);
  root_plane_.setSmoothScroll(display.smooth_scroll);
  root_plane_.setCellBorders(display.cell_borders);
  root_plane_.setKeymapProfile(keymap_override_.value_or(display.keymap_profile));
  root_plane_.setFlatChildren(true);
  root_plane_.setThemedBases(true);
  key_reader_.setEscapeAsMeta(display.escape_as_meta);
  if (display.pictures && display.cell_px_height > 0 && display.cell_px_width > 0) {
    root_plane_.setCellPixelSize(display.cell_px_height, display.cell_px_width);
  } else {
    root_plane_.setCellPixelSize(0, 0);
  }
  charts_->reset(display);
}

void Application::setRootWidget(Widget & root) {
  root_widget_ = &root;
  dispatcher_ = std::make_unique<InputDispatcher>(root);
  dispatcher_->setDragDropEnabled(backend_->display().drag_drop);
  installRootHandlers();
}

void Application::setKeymapProfile(KeymapProfile profile) {
  keymap_override_ = profile;
  root_plane_.setKeymapProfile(profile);
}

void Application::resizeTo(int rows, int cols) {
  if (rows == buffer_.rows() && cols == buffer_.cols()) return;
  buffer_ = CellBuffer(rows, cols);
  root_plane_.resize(rows, cols);
  layoutPalette();
  resized.emit(rows, cols);
}

void Application::handle(const BackendEvent & event) {
  switch (event.type) {
    case BackendEvent::Type::CLOSE:
      if (quit_on_close_) quit();
      closed.emit();
      break;
    case BackendEvent::Type::RESIZE: resizeTo(backend_->rows(), backend_->cols()); break;
    case BackendEvent::Type::DISPLAY:
      configurePlane();
      resizeTo(backend_->rows(), backend_->cols());
      layoutPalette();
      display_changed.emit();
      break;
    case BackendEvent::Type::INPUT: {
      // Through the reader, which folds a bare Escape and the key after it into one Meta key.
      key_reader_.push(event.input);
      Input in(0, -1, -1, false, false, false, false);
      while (key_reader_.next(&in)) offerInput(in);
      break;
    }
    case BackendEvent::Type::PASTE:
      if (!root_plane_.insertIntoReader(event.text)) {
        std::string line = event.text;
        std::replace(line.begin(), line.end(), '\n', ' ');
        dispatcher_->paste(Paste{ std::move(line) });
      }
      break;
    case BackendEvent::Type::FOCUS:
      styles_->window_focused = event.focused;
      focus_changed.emit(event.focused);
      break;
    default: break;
  }
}

void Application::offerInput(const Input & in) {
  if (escape_echoed_ && !key_reader_.escapePending()) {
    // The key the Escape waited for: the echo goes before what that key has to say.
    escape_echoed_ = false;
    message.emit("");
  }
  if (input_hook_ && input_hook_(in)) return;
  if (dispatcher_->offer(in) == Handled::yes) return;
  // Tab that nothing took walks the focus, as in any toolkit.
  if (in.getId() == Key::Tab && in.getKind() != Input::Kind::RELEASE && !in.hasCtrl() && !in.hasAlt()) {
    if (in.hasShift()) dispatcher_->focusPrev();
    else dispatcher_->focusNext();
  }
}

void Application::present() {
  auto now = std::chrono::steady_clock::now();
  double seconds = last_frame_ ? std::min(0.1, std::chrono::duration<double>(now - *last_frame_).count()) : 0.0;
  last_frame_ = now;
  frame_started.emit(seconds);
  // The open palette's list eases where the display scrolls smoothly: it moves with each frame and asks for the next.
  if (palette_.isOpen() && palette_.animating()) {
    palette_.tick(seconds);
    layoutPalette();
    if (palette_.animating()) backend_->requestFrame();
  }
  if (before_refresh_) before_refresh_();
  root_plane_.refresh();
  dispatcher_->setHitAreas(root_plane_.takeHitAreas());
  CursorRequest cursor;
  if (auto at = root_plane_.cursorPosition()) cursor = {true, at->first, at->second};
  FrameInfo info;
  info.backdrops = root_plane_.takeBackdropLayers();
  info.elevations = root_plane_.takeElevations();
  info.text_runs = root_plane_.takeTextRuns();
  info.mouse_cursors = root_plane_.takeMouseCursors();
  info.borders = root_plane_.takeBorders();
  if (backend_->wantsFrameInfo()) {
    info.planes = root_plane_.describe();
    if (focus_name_) info.focus = focus_name_();
  }
  backend_->presentTaking(buffer_, cursor, root_plane_.images(), std::move(info));
}

int Application::run() {
  EventLoop & loop = backend_->eventLoop();
  backend_->setFrameHandler([this](int64_t) {
    if (!frame_hook_ || frame_hook_()) present();
  });
  // A wake with nothing from the display (another thread's post) asks for no frame by itself.
  auto pump = [this] {
    std::vector<BackendEvent> events = backend_->poll();
    for (const BackendEvent & event : events) handle(event);
    if (!events.empty()) backend_->requestFrame();
  };
  // A pending Escape that no key followed is held until the key reader says otherwise.
  loop.addSource({backend_->inputFd(), backend_->wakeFd()}, pump);
  if (auto handles = backend_->inputHandles(); !handles.empty()) loop.addHandles(handles, pump);
  // The backend's own clock: what the terminal's input holds of a sequence a read split (a lone ESC byte) is
  // settled when its grace runs out, and a frame the pacer held back starts when it may; and the "ESC-" echo is
  // due a moment into a bare Escape's wait.
  using Clock = std::chrono::steady_clock;
  auto msUntil = [](Clock::time_point t) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t - Clock::now()).count() + 1;
    return static_cast<int>(std::clamp<long long>(ms, 0, 60000));
  };
  loop.setBefore([this, msUntil] {
    int wait = -1;
    auto sooner = [&](int ms) { wait = wait < 0 ? ms : std::min(wait, ms); };
    if (auto deadline = backend_->nextDeadline()) sooner(msUntil(*deadline));
    if (auto due = key_reader_.indicatorDue(); due && !escape_echoed_) sooner(msUntil(*due));
    if (before_turn_) {
      if (int own = before_turn_(); own >= 0) sooner(own);
    }
    return wait;
  });
  loop.setAfter([this, pump](bool woken) {
    if (!woken) {
      if (auto deadline = backend_->nextDeadline(); deadline && Clock::now() >= *deadline) pump();
    }
    echoEscape();
    if (after_turn_) after_turn_(woken);
  });
  backend_->requestFrame();
  exit_status_ = 0;
  loop.run();
  loop.setBefore(nullptr);
  loop.setAfter(nullptr);
  return exit_status_;
}

// Emacs's "ESC-" in the echo area while a bare Escape waits for the key it may be the Meta of.
void Application::echoEscape() {
  auto due = key_reader_.indicatorDue();
  if (due && !escape_echoed_ && std::chrono::steady_clock::now() >= *due) {
    escape_echoed_ = true;
    message.emit("ESC-");
    backend_->requestFrame();
  }
}

void Application::quit(int status) {
  exit_status_ = status;
  backend_->eventLoop().quit();
}

}  // namespace cellui
