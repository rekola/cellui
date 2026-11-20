#pragma once

#include <cellui/Backend.h>
#include <cellui/BackendChoice.h>
#include <cellui/BufferPlane.h>
#include <cellui/Charts.h>
#include <cellui/CommandPalette.h>
#include <cellui/CommandRegistry.h>
#include <cellui/InputDispatcher.h>
#include <cellui/KeyReader.h>
#include <cellui/Keymap.h>
#include <cellui/Signal.h>
#include <cellui/StyleProvider.h>
#include <cellui/Widget.h>
#include <cellui/term/ImageEncoder.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cellui {

// What a program chooses about its display, for one that reads its own command
// line (Application::open(argc, argv) fills the first two from `--ui` and
// `--theme`).
struct ApplicationOptions {
  BackendKind ui = BackendKind::AUTO;
  // A Theme::byName() name; empty follows the display. Ignored with `styles`.
  std::string theme;
  // The style provider every plane draws with, for a program with colours of
  // its own (a class derived from StyleProvider, whose colours are then the
  // program's to set). Empty: a plain one in `theme`.
  std::unique_ptr<StyleProvider> styles;
  // The object planes carry for the program (Plane::getAppContext()); it can
  // also be given later (root().setAppContext()).
  std::shared_ptr<void> app_context;
  // The keymap profile to use instead of the display's own.
  std::optional<KeymapProfile> keymap;
  // How what has to meet a terminal's palette (Sixel pictures) is dithered.
  std::optional<term::Dither::Method> dither;
};

// A program's whole connection to the display: it opens the backend (a
// terminal or a window, whichever the machine and the command line ask for),
// owns the screen-sized root plane, the input dispatcher and the loop, keeps
// the root plane the size of the display, and draws a frame whenever
// something asked for one. The program names no platform and no backend; it
// builds its widgets on root(), lays them out from rows() and cols() (and
// again on `resized`), and calls run().
//
//   auto app = cellui::Application::open(argc, argv, &error);
//   if (!app) { /* report error */ return 1; }
//   Table table(app->root(), app->charts());
//   table.attach(app->widgets());
//   auto resized = app->resized.connect([&](int rows, int cols) { table.resize(rows, cols); });
//   app->commands().define("quit", "Leave the program", [&] { app->quit(); });
//   app->keymap().bind("C-x C-c", "quit");
//   return app->run();
//
// It also does what every program on cellui does with the display it got: the
// plane's attributes from DisplayInfo (colours, scale, pictures, keymap
// profile), a theme to suit the display's colours (`--theme` names one),
// escape-as-Meta key reading, Tab to move the focus, paste into an open text
// field, the window's focus in the style provider, and drag and drop.
//
// And it runs the program's commands: named actions (commands()) that the keys
// bound to them (keymap()) and the command palette run. A key goes to the
// focused widget first; one it does not take is looked up in the keymap, and
// one nothing takes moves the focus if it is Tab or Shift+Tab. A prefix (C-x)
// takes the key after it whatever has the focus. "command-palette" is defined
// and bound already (M-x on the terminal keymap, Ctrl+Shift+P on the desktop
// one); while the palette is open it has every key and pointer event, Escape
// included at once.
//
// The command line: `--ui auto|terminal|window` (or `--ui=...`) picks the
// backend, `auto` (the default) being a window where the build has one and a
// display is there and the terminal otherwise. `--theme original|dark|light` overrides the theme. Everything
// else is left in arguments() for the program.
//
// A program with a loop of its own (a tracker with audio and MIDI sources)
// does not call run(): it uses the display's loop (backend().eventLoop()),
// hands each Backend::poll() event to handle() and draws with present() from
// its frame handler. The program's own colours, object and key handling go in
// through ApplicationOptions and setInputHook().
class Application {
 public:
  // Opens the display `argv` asks for. nullptr, with *error set, when a named
  // backend cannot start, when none of the automatic candidates can, or when
  // `--ui` is not understood.
  static std::unique_ptr<Application> open(int argc, char ** argv, std::string * error = nullptr);
  // The same for a program that has read its own command line.
  static std::unique_ptr<Application> open(ApplicationOptions options, std::string * error = nullptr);
  // The same over a backend the caller opened (a test's).
  static std::unique_ptr<Application> adopt(std::unique_ptr<Backend> backend, ApplicationOptions options = {});
  ~Application();
  Application(const Application &) = delete;
  Application & operator=(const Application &) = delete;

  // What was left of the command line once the toolkit's options were taken,
  // without the program name.
  const std::vector<std::string> & arguments() const { return arguments_; }

  // The screen-sized root plane to build widgets on, and the root of their
  // tree (attach() a widget to it to give it keys, the mouse and focus).
  BufferPlane & root() { return root_plane_; }
  Widget & widgets() { return *root_widget_; }
  // Makes `root` the root of the widget tree instead of the application's own, for a program whose top-level
  // widget is the one that takes modal dialogs and posted events. Call it before anything uses input().
  void setRootWidget(Widget & root);
  InputDispatcher & input() { return *dispatcher_; }
  KeyReader & keyReader() { return key_reader_; }
  // The style provider planes draw with: the one in the options, else the toolkit's.
  StyleProvider & styles() { return *styles_; }
  // The display's charts. The reference stays valid when the display changes.
  const Charts & charts() const;
  Backend & backend() { return *backend_; }
  const DisplayInfo & display() const { return backend_->display(); }
  // Which backend is running, and whether its display is light (a window by
  // its background colour, a terminal by what it reports it is).
  BackendKind kind() const { return kind_; }
  bool light() const { return light_; }
  int rows() const { return buffer_.rows(); }
  int cols() const { return buffer_.cols(); }

  // The display changed size: the root plane already has the new one.
  Signal<int, int> resized;
  // The display's description changed (cell size, scale, pictures): charts()
  // is already the new display's, and widgets that drew pictures draw again.
  Signal<> display_changed;
  // A frame is about to be drawn: the seconds since the one before (at most a tenth, 0 for the first), for what
  // animates (a table easing to its scroll position). A widget that is still moving asks for the next frame
  // (requestFrame()) from its handler.
  Signal<double> frame_started;
  // The window got or lost the keyboard focus (styles().window_focused is set).
  Signal<bool> focus_changed;
  // The display went away: the end of input, or the window was closed.
  Signal<> closed;

  // What the application has to say on the echo line: a prefix waiting for its
  // key ("C-x-"), a key sequence that is not bound, a bare Escape waiting on a
  // terminal for the key it may be the Meta of ("ESC-"). The program shows it where
  // it shows messages (StatusLine::setMessage()).
  Signal<std::string> message;

  // --- Commands
  // The program's named commands and the keys bound to them (see above).
  // Define a command with a description, which the palette shows.
  CommandRegistry & commands() { return commands_; }
  Keymap & keymap() { return keymap_; }
  // Runs a command by name (commands(), then the command source's); false when there is none.
  bool execute(const std::string & name);
  // How `command` is bound on this display ("C-x C-c", "Ctrl+Q"; keymap(), then the command source's); "" if it is not.
  std::string bindingFor(const std::string & command) const;

  // Commands the program keeps itself, for one whose commands are not all in
  // commands(): spread over several registries, or run differently by what
  // has the focus. The palette lists them with commands()'s, and a name that
  // commands() and keymap() do not have is run, described and bound by the
  // source. A part left empty adds nothing.
  struct CommandSource {
    std::function<std::vector<std::string>()> names;
    std::function<std::string(const std::string &)> description;
    std::function<std::string(const std::string &)> binding;
    std::function<bool(const std::string &)> execute;
  };
  void setCommandSource(CommandSource source) { command_source_ = std::move(source); }
  // Opens the command palette over the bottom of the screen with every command;
  // the chosen one runs once the palette has closed.
  void openCommandPalette();
  bool commandPaletteOpen() const { return palette_.isOpen(); }
  const CommandPalette & commandPalette() const { return palette_; }

  // Offered every key and mouse input before the dispatcher; returns whether
  // it took it. A last resort: a key binding (keymap()) or a capture handler on
  // widgets() does what a program needs without the display's raw input.
  using InputHook = std::function<bool(const Input &)>;
  void setInputHook(InputHook hook) { input_hook_ = std::move(hook); }

  // Set another theme (a Theme::byName() name; false if unknown).
  bool setTheme(const std::string & name);
  // The keymap profile to use instead of the display's, from now on.
  void setKeymapProfile(KeymapProfile profile);

  // Takes what the display says: an input, a resize, a paste, a change of
  // display, the window's focus, the end. run() does it for its own events; a
  // program with a loop of its own calls it for each event of backend().poll().
  void handle(const BackendEvent & event);
  // Draws what the tree shows and gives it to the display (a frame handler
  // does it). `before_refresh` runs first (raise popups); `focus_name` names
  // the focus in a test's view of the frame.
  void present();
  void setBeforeRefresh(std::function<void()> hook) { before_refresh_ = std::move(hook); }
  void setFocusName(std::function<std::string()> name) { focus_name_ = std::move(name); }
  // Whether the end of input or a closed window ends run() (the default), or
  // is only said by `closed`.
  void setQuitOnClose(bool quit) { quit_on_close_ = quit; }

  // --- A program's own work in run()'s loop (synth: sources of its own, a frame
  // rate of its own, statistics). Each runs alongside the application's own.
  // As a turn starts: the longest the turn may wait, in milliseconds (negative:
  // as long as it takes; the sooner of this and the application's own wins).
  void setBeforeTurn(std::function<int()> hook) { before_turn_ = std::move(hook); }
  // As a turn ends, with whether a source woke it.
  void setAfterTurn(std::function<void(bool woken)> hook) { after_turn_ = std::move(hook); }
  // When the display lets a frame start: draws what the program has to and
  // says whether there is anything new to show; false skips the frame, so a
  // program that redraws only what changed sends nothing when nothing did.
  // Unset, every frame is shown.
  void setFrameHook(std::function<bool()> hook) { frame_hook_ = std::move(hook); }

  // Asks for a frame; call after changing anything outside an input handler
  // (data that arrived).
  void requestFrame() { backend_->requestFrame(); }

  // Runs until quit() or the display closes. Returns the status given to quit(), 0 when the display closed.
  // A frame is asked for when the display reported something (an input, a resize); a source of the program's own
  // or a post from another thread (ThreadInbox) asks for one itself (requestFrame()) when it changed what shows.
  // The program's own sources are added to backend().eventLoop() before run().
  int run();
  void quit(int status = 0);

 private:
  Application(std::unique_ptr<Backend> backend, std::vector<std::string> arguments, ApplicationOptions options, BackendKind kind, bool light);
  void configurePlane();
  void applyTheme(const std::string & name);
  void offerInput(const Input & input);
  void resizeTo(int rows, int cols);
  void installRootHandlers();
  // The key the focused widget left: a binding of the keymap, or the start of a prefix.
  bool keyBinding(const KeyDown & e);
  // The key after a prefix, whatever has the focus.
  bool prefixedKey(const KeyDown & e);
  void layoutPalette();
  void echoEscape();
  void paletteResult(CommandPalette::Result result);
  KeymapProfile profile() const { return root_plane_.keymapProfile(); }

  class DisplayCharts;

  std::unique_ptr<Backend> backend_;
  std::vector<std::string> arguments_;
  bool own_theme_;  // before styles_: the options' styles are moved out in making it
  std::unique_ptr<StyleProvider> styles_;
  BackendKind kind_;
  bool light_;
  CellBuffer buffer_;
  BufferPlane root_plane_;
  std::unique_ptr<DisplayCharts> charts_;
  Widget own_root_widget_;
  Widget * root_widget_ = &own_root_widget_;
  std::unique_ptr<InputDispatcher> dispatcher_;
  InputHook input_hook_;
  KeyReader key_reader_;
  std::optional<KeymapProfile> keymap_override_;
  std::function<void()> before_refresh_;
  std::function<std::string()> focus_name_;
  bool quit_on_close_ = true;
  std::string theme_name_;
  std::optional<std::chrono::steady_clock::time_point> last_frame_;
  int exit_status_ = 0;
  std::function<int()> before_turn_;
  std::function<void(bool)> after_turn_;
  std::function<bool()> frame_hook_;
  bool escape_echoed_ = false;
  CommandRegistry commands_;
  Keymap keymap_;
  CommandPalette palette_;
  // The prefix key (C-x) of a sequence, while it waits for the key after it.
  std::optional<uint64_t> pending_prefix_;
  CommandSource command_source_;
  std::vector<Connection> root_handlers_;
};

}  // namespace cellui
