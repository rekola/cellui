#pragma once

#include <cellui/Backend.h>
#include <cellui/BackendChoice.h>
#include <cellui/BufferPlane.h>
#include <cellui/Charts.h>
#include <cellui/InputDispatcher.h>
#include <cellui/KeyReader.h>
#include <cellui/Signal.h>
#include <cellui/Widget.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cellui {

// An application's whole connection to the display: it opens the backend (a
// terminal or a window, whichever the machine and the command line ask for),
// owns the screen-sized root plane, the input dispatcher and the loop, keeps
// the root plane the size of the display, and draws a frame whenever something
// asked for one. The program names no platform and no backend; it builds its
// widgets on root(), lays them out from rows() and cols() (and again on
// `resized`), and calls run().
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
// display is there and the terminal otherwise; `raster` is a second name for
// `window`. `--theme original|dark|light` overrides the theme. Everything
// else is left in arguments() for the program.
class Application {
 public:
  // Opens the display `argv` asks for. nullptr, with *error set, when a named
  // backend cannot start, when none of the automatic candidates can, or when
  // `--ui` is not understood.
  static std::unique_ptr<Application> open(int argc, char ** argv, std::string * error = nullptr);
  // `theme` is a Theme::byName() name, or empty to follow the display.
  // The same over a backend the caller opened (a test's, or one with options
  // of its own).
  static std::unique_ptr<Application> adopt(std::unique_ptr<Backend> backend, const std::string & theme = "");
  ~Application();
  Application(const Application &) = delete;
  Application & operator=(const Application &) = delete;

  // What was left of the command line once the toolkit's options were taken,
  // without the program name.
  const std::vector<std::string> & arguments() const { return arguments_; }

  // The screen-sized root plane to build widgets on, and the root of their
  // tree (attach() a widget to it to give it keys, the mouse and focus).
  Plane & root() { return root_plane_; }
  Widget & widgets() { return root_widget_; }
  InputDispatcher & input() { return dispatcher_; }
  // The display's charts. The reference stays valid when the display changes.
  const Charts & charts() const;
  Backend & backend() { return *backend_; }
  const DisplayInfo & display() const { return backend_->display(); }
  int rows() const { return buffer_.rows(); }
  int cols() const { return buffer_.cols(); }

  // The display changed size: the root plane already has the new one.
  Signal<int, int> resized;
  // The display's description changed (cell size, scale, pictures): charts()
  // is already the new display's, and widgets that drew pictures draw again.
  Signal<> display_changed;

  // Offered every key and mouse input before the dispatcher; returns whether
  // it took it (a global shortcut).
  using InputHook = std::function<bool(const Input &)>;
  void setInputHook(InputHook hook) { input_hook_ = std::move(hook); }

  // The style provider every widget of the tree draws with. Colours follow
  // the theme chosen at start; set another with setTheme() (a Theme::byName()
  // name; false if unknown).
  StyleProvider & styles() { return styles_; }
  bool setTheme(const std::string & name);

  // Draws a frame when the display lets one start. Call after changing
  // anything outside an input handler (data that arrived).
  void requestFrame() { backend_->requestFrame(); }

  // Runs until quit() or the display closes. Returns the exit status.
  int run();
  void quit();

 private:
  Application(std::unique_ptr<Backend> backend, std::vector<std::string> arguments, const std::string & theme);
  void applyTheme(const std::string & name);
  void offerInput(const Input & input);
  void configurePlane();
  void handle(const BackendEvent & event);
  void frame();
  void resizeTo(int rows, int cols);

  class DisplayCharts;

  std::unique_ptr<Backend> backend_;
  std::vector<std::string> arguments_;
  StyleProvider styles_;
  CellBuffer buffer_;
  BufferPlane root_plane_;
  std::unique_ptr<DisplayCharts> charts_;
  Widget root_widget_;
  InputDispatcher dispatcher_;
  InputHook input_hook_;
  KeyReader key_reader_;
  std::string theme_name_;
};

}  // namespace cellui
