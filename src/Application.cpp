#include <cellui/Application.h>

#include <cellui/BackendChoice.h>
#ifdef CELLUI_HAS_GTK
#include <cellui/gtk/Window.h>
#endif
#ifdef CELLUI_HAS_WIN32
#include <cellui/win32/Window.h>
#endif
#ifndef _WIN32
#include <cellui/term/TerminalSession.h>
#endif

#include <cstring>
#include <string_view>

namespace cellui {

// The display's charts behind one stable object: widgets keep a reference to
// it, and it follows the display when its pictures, cell size or scale change.
class Application::DisplayCharts : public Charts {
 public:
  void reset(const DisplayInfo & display) { inner_ = chartsFor(display); }

  std::shared_ptr<LineChart> lineChart(Plane & parent) const override { return inner_->lineChart(parent); }
  std::shared_ptr<HeatmapChart> heatmap(Plane & parent, int grid_cols, int grid_rows) const override {
    return inner_->heatmap(parent, grid_cols, grid_rows);
  }
  void drawSparkline(Plane & plane, int row, int col, int rows, const std::vector<SparklineReading> & readings, const SparklineLook & look,
                     int clip_top, int clip_bottom, uint64_t id) const override {
    inner_->drawSparkline(plane, row, col, rows, readings, look, clip_top, clip_bottom, id);
  }
  void drawTrace(Plane & plane, int row, int col, int rows, int cols, const std::vector<float> & values, const TraceLook & look, int clip_top,
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

std::unique_ptr<Backend> openOne(BackendKind kind, std::string * error) {
  switch (kind) {
    case BackendKind::RASTER:
#if defined(CELLUI_HAS_GTK)
      return gtk::Window::open({}, error);
#elif defined(CELLUI_HAS_WIN32)
      return win32::Window::open({}, error);
#else
      if (error) *error = "this build has no window backend";
      return nullptr;
#endif
    case BackendKind::TERMINAL:
#ifndef _WIN32
      return term::TerminalSession::open(term::SessionOptions(), error);
#else
      if (error) *error = "there is no terminal backend on Windows";
      return nullptr;
#endif
    case BackendKind::AUTO: break;
  }
  if (error) *error = "no backend";
  return nullptr;
}

}  // namespace

std::unique_ptr<Application> Application::open(int argc, char ** argv, std::string * error) {
  BackendKind requested = BackendKind::AUTO;
  std::string theme;
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
    if (!is_ui && arg == "--theme" && i + 1 < argc) {
      theme = argv[++i];
      Theme probe;
      if (!Theme::byName(theme, probe)) {
        if (error) *error = "unknown --theme '" + theme + "': use original, dark or light";
        return nullptr;
      }
      continue;
    }
    if (!is_ui) {
      rest.emplace_back(arg);
      continue;
    }
    if (!parseBackendKind(name, requested)) {
      if (error) *error = "unknown --ui '" + std::string(name) + "': use auto, terminal or window";
      return nullptr;
    }
  }

  std::string failure;
  for (BackendKind kind : backendCandidates(requested, probeBackendEnvironment())) {
    std::string why;
    if (auto backend = openOne(kind, &why)) return std::unique_ptr<Application>(new Application(std::move(backend), std::move(rest), theme));
    failure += (failure.empty() ? "" : "; ") + std::string(backendName(kind)) + ": " + why;
  }
  if (error) *error = failure;
  return nullptr;
}

std::unique_ptr<Application> Application::adopt(std::unique_ptr<Backend> backend, const std::string & theme) {
  return std::unique_ptr<Application>(new Application(std::move(backend), {}, theme));
}

Application::Application(std::unique_ptr<Backend> backend, std::vector<std::string> arguments, const std::string & theme)
    : backend_(std::move(backend)),
      arguments_(std::move(arguments)),
      buffer_(backend_->rows(), backend_->cols()),
      root_plane_(buffer_, styles_),
      charts_(std::make_unique<DisplayCharts>()),
      dispatcher_(root_widget_) {
  applyTheme(theme);
  configurePlane();
  dispatcher_.setDragDropEnabled(backend_->display().drag_drop);
  // A bare Escape waits for the key after it, in case it was an Alt prefix,
  // unless what has the focus wants it at once.
  key_reader_.takeEscape = [this](const Input & escape) {
    Widget * focused = dispatcher_.focused();
    if (!focused || !focused->wantsBareEscape()) return false;
    dispatcher_.offer(escape);
    return true;
  };
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
  styles_.applyTheme(theme);
}

bool Application::setTheme(const std::string & name) {
  Theme probe;
  if (!Theme::byName(name, probe)) return false;
  applyTheme(name);
  root_plane_.invalidateSharedImages();
  return true;
}

void Application::configurePlane() {
  const DisplayInfo & display = backend_->display();
  if (display.default_fg && display.default_bg) root_plane_.setDefaultColors(*display.default_fg, *display.default_bg);
  root_plane_.setProportionalText(display.proportional_text);
  root_plane_.setSmoothScroll(display.smooth_scroll);
  root_plane_.setCellBorders(display.cell_borders);
  root_plane_.setKeymapProfile(display.keymap_profile);
  root_plane_.setFlatChildren(true);
  root_plane_.setThemedBases(true);
  key_reader_.setEscapeAsMeta(display.escape_as_meta);
  if (display.pictures && display.cell_px_height > 0 && display.cell_px_width > 0) {
    root_plane_.setCellPixelSize(display.cell_px_height, display.cell_px_width);
    root_plane_.setDisplayScale(display.display_scale);
    root_plane_.setHdrPictures(display.hdr_pictures);
  } else {
    root_plane_.setCellPixelSize(0, 0);
    root_plane_.setHdrPictures(false);
  }
  charts_->reset(display);
}

void Application::resizeTo(int rows, int cols) {
  if (rows == buffer_.rows() && cols == buffer_.cols()) return;
  buffer_ = CellBuffer(rows, cols);
  root_plane_.resize(rows, cols);
  resized.emit(rows, cols);
}

void Application::handle(const BackendEvent & event) {
  switch (event.type) {
    case BackendEvent::Type::CLOSE: quit(); break;
    case BackendEvent::Type::RESIZE: resizeTo(backend_->rows(), backend_->cols()); break;
    case BackendEvent::Type::DISPLAY:
      configurePlane();
      resizeTo(backend_->rows(), backend_->cols());
      display_changed.emit();
      break;
    case BackendEvent::Type::INPUT: {
      // Through the reader, which folds a bare Escape and the key after it into one Meta key.
      key_reader_.push(event.input);
      Input in(0, -1, -1, false, false, false, false);
      while (key_reader_.next(&in)) offerInput(in);
      break;
    }
    case BackendEvent::Type::PASTE: root_plane_.insertIntoReader(event.text); break;
    case BackendEvent::Type::FOCUS: styles_.window_focused = event.focused; break;
    default: break;
  }
}

void Application::offerInput(const Input & in) {
  if (input_hook_ && input_hook_(in)) return;
  // Tab walks the focus, as in any toolkit.
  if (in.getId() == Key::Tab && in.getKind() != Input::Kind::RELEASE && !in.hasCtrl() && !in.hasAlt()) {
    if (in.hasShift()) dispatcher_.focusPrev();
    else dispatcher_.focusNext();
    return;
  }
  dispatcher_.offer(in);
}

void Application::frame() {
  root_plane_.refresh();
  dispatcher_.setHitAreas(root_plane_.takeHitAreas());
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
  }
  backend_->presentTaking(buffer_, cursor, root_plane_.images(), std::move(info));
}

int Application::run() {
  EventLoop & loop = backend_->eventLoop();
  backend_->setFrameHandler([this](int64_t) { frame(); });
  auto pump = [this] {
    for (const BackendEvent & event : backend_->poll()) handle(event);
    backend_->requestFrame();
  };
  // A pending Escape that no key followed is held until the key reader says otherwise.
  loop.addSource({backend_->inputFd(), backend_->wakeFd()}, pump);
  if (auto handles = backend_->inputHandles(); !handles.empty()) loop.addHandles(handles, pump);
  backend_->requestFrame();
  loop.run();
  return 0;
}

void Application::quit() { backend_->eventLoop().quit(); }

}  // namespace cellui
