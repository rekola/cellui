#pragma once

#include <cellui/Backend.h>
#include <cellui/BackendChoice.h>
#include <cellui/BufferPlane.h>
#include <cellui/Charts.h>
#include <cellui/InputDispatcher.h>
#include <cellui/KeyReader.h>
#include <cellui/Signal.h>
#include <cellui/StyleProvider.h>
#include <cellui/Widget.h>
#include <cellui/term/ImageEncoder.h>

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
//   app->resized.connect([&](int rows, int cols) { table.resize(rows, cols); });
//   return app->run();
//
// It also does what every program on cellui does with the display it got: the
// plane's attributes from DisplayInfo (colours, scale, pictures, keymap
// profile), a theme to suit the display's colours (`--theme` names one),
// escape-as-Meta key reading, Tab to move the focus, paste into an open text
// field, the window's focus in the style provider, and drag and drop.
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
  // The window got or lost the keyboard focus (styles().window_focused is set).
  Signal<bool> focus_changed;
  // The display went away: the end of input, or the window was closed.
  Signal<> closed;

  // Offered every key and mouse input before the dispatcher; returns whether
  // it took it (a global shortcut).
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

  // Asks for a frame; call after changing anything outside an input handler
  // (data that arrived).
  void requestFrame() { backend_->requestFrame(); }

  // Runs until quit() or the display closes. Returns the exit status.
  int run();
  void quit();

 private:
  Application(std::unique_ptr<Backend> backend, std::vector<std::string> arguments, ApplicationOptions options, BackendKind kind, bool light);
  void configurePlane();
  void applyTheme(const std::string & name);
  void offerInput(const Input & input);
  void resizeTo(int rows, int cols);

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
};

}  // namespace cellui
