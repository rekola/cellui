#include "TestFramework.h"

#include <cellui/Application.h>
#include <cellui/InfoModal.h>
#include <cellui/TestBackend.h>

using namespace cellui;

namespace {

struct Fixture {
  test::TestBackend * backend;
  std::unique_ptr<Application> app;
  Fixture(int rows = 10, int cols = 40) {
    auto owned = std::make_unique<test::TestBackend>(rows, cols);
    backend = owned.get();
    app = Application::adopt(std::move(owned));
  }
  // Runs the loop until the test backend has no more to say.
  void turn() { app->backend().eventLoop().iterate(false); }
};

}  // namespace

TEST(application_root_starts_at_the_display_size) {
  Fixture f(10, 40);
  CHECK(f.app->rows() == 10 && f.app->cols() == 40);
  CHECK(f.app->root().getDim() == std::pair(10, 40));
  CHECK(f.app->arguments().empty());
}

TEST(application_runs_draws_a_widget_and_keeps_it_filling_after_a_resize) {
  Fixture f(6, 30);
  Widget bar(f.app->root());
  bar.attach(f.app->widgets());
  bar.resize(f.app->rows(), f.app->cols());
  bar.setFgColor(Color(255, 255, 255));
  bar.putstr(0, 0, "hello");
  int signalled_rows = 0;
  auto c = f.app->resized.connect([&](int rows, int cols) {
    signalled_rows = rows;
    bar.resize(rows, cols);
  });
  f.backend->resize(9, 50);
  f.backend->pushKey('q');
  f.app->setInputHook([&](const Input & in) {
    if (in.getId() == 'q') f.app->quit();
    return in.getId() == 'q';
  });
  CHECK(f.app->run() == 0);
  CHECK(f.app->rows() == 9 && f.app->cols() == 50);
  CHECK(signalled_rows == 9);
  CHECK(bar.getDim() == std::pair(9, 50));
  CHECK(f.backend->text().find("hello") != std::string::npos);
}

TEST(application_tab_moves_the_focus_between_focusable_widgets) {
  Fixture f;
  Widget a(f.app->root()), b(f.app->root());
  a.setFocusable(true);
  b.setFocusable(true);
  a.attach(f.app->widgets());
  b.attach(f.app->widgets());
  f.app->input().setFocus(&a);
  f.backend->pushKey(Key::Tab);
  f.backend->pushKey('q');
  f.app->setInputHook([&](const Input & in) {
    if (in.getId() == 'q') f.app->quit();
    return in.getId() == 'q';
  });
  f.app->run();
  CHECK(f.app->input().focused() == &b);
}

TEST(application_chooses_a_theme_for_a_light_display_and_a_named_one) {
  Fixture f;
  CHECK(f.app->setTheme("light"));
  CHECK(!f.app->setTheme("nonesuch"));
}

namespace {
struct OwnStyles : StyleProvider {
  int marker = 42;
};
}  // namespace

TEST(application_takes_the_programs_styles_and_object_and_serves_its_own_loop) {
  auto owned = std::make_unique<test::TestBackend>(8, 30);
  test::TestBackend * backend = owned.get();
  ApplicationOptions options;
  options.styles = std::make_unique<OwnStyles>();
  options.app_context = std::make_shared<int>(7);
  auto app = Application::adopt(std::move(owned), std::move(options));
  CHECK(static_cast<OwnStyles &>(app->styles()).marker == 42);
  CHECK(&app->root().getStyles() == &app->styles());
  CHECK(*std::static_pointer_cast<int>(app->root().getAppContext()) == 7);
  CHECK(!app->setTheme("light"));  // the program's own colours are its own to set

  // What a loop of its own does: hand over each event, draw from its frame handler.
  bool focus = true, closed = false, refreshed = false;
  auto a = app->focus_changed.connect([&](bool f) { focus = f; });
  auto b = app->closed.connect([&] { closed = true; });
  app->setBeforeRefresh([&] { refreshed = true; });
  app->setQuitOnClose(false);
  app->handle(BackendEvent::makeFocus(false));
  CHECK(!focus && !app->styles().window_focused);
  app->present();
  CHECK(refreshed);
  CHECK(backend->frameCount() == 1u);
  int rows = 0;
  auto c = app->resized.connect([&](int r, int) { rows = r; });
  backend->resize(12, 40);
  app->handle(BackendEvent::makeResize(12, 40));
  CHECK(rows == 12 && app->root().getDim() == std::pair(12, 40));
  BackendEvent end;
  end.type = BackendEvent::Type::CLOSE;
  app->handle(end);
  CHECK(closed);
}

TEST(application_can_use_the_programs_top_widget_as_its_root) {
  Fixture f;
  Widget top;
  Widget child(f.app->root());
  child.setFocusable(true);
  f.app->setRootWidget(top);
  child.attach(top);
  CHECK(&f.app->widgets() == &top);
  f.app->input().setFocus(&child);
  CHECK(f.app->input().focused() == &child);
}

TEST(application_says_how_long_since_the_last_frame_before_drawing_one) {
  Fixture f;
  std::vector<double> seconds;
  auto c = f.app->frame_started.connect([&](double s) { seconds.push_back(s); });
  f.app->present();
  f.app->present();
  CHECK(seconds.size() == 2u);
  CHECK(seconds[0] == 0.0);  // none before the first
  CHECK(seconds[1] >= 0.0 && seconds[1] <= 0.1);
}

namespace {
// Presses a key the way the display reports one, and draws.
void press(Application & app, int key, unsigned mods = 0) {
  app.handle(BackendEvent::makeInput(keyInput(key, mods, Input::Kind::PRESS)));
  app.present();
}
}  // namespace

TEST(application_runs_commands_by_their_keys_and_prefixes_take_the_next_key) {
  Fixture f;
  int saves = 0, quits = 0;
  std::vector<std::string> said;
  auto m = f.app->message.connect([&](std::string s) { said.push_back(std::move(s)); });
  f.app->commands().define("save", "Save", [&] { saves++; });
  f.app->commands().define("quit", "Quit", [&] { quits++; });
  f.app->keymap().bind("C-s", "save");
  f.app->keymap().bind("C-x C-c", "quit");
  press(*f.app, 's', kModCtrl);
  CHECK(saves == 1);
  press(*f.app, 'x', kModCtrl);
  CHECK(!said.empty() && said.back() == "C-x-");
  press(*f.app, 'c', kModCtrl);
  CHECK(quits == 1);
  press(*f.app, 'x', kModCtrl);
  press(*f.app, 's', kModCtrl);  // C-x C-s is not bound: the prefix takes it, nothing runs
  CHECK(saves == 1 && said.back() == "C-x C-s is undefined");
  CHECK(f.app->bindingFor("quit") == "C-x C-c");
}

TEST(application_gives_a_key_to_the_focused_widget_before_the_keymap) {
  Fixture f;
  Widget w(f.app->root());
  w.setFocusable(true);
  w.attach(f.app->widgets());
  f.app->input().setFocus(&w);
  int widget_keys = 0, commands = 0;
  auto c = w.on<KeyDown>([&](const KeyDown & e, EventContext &) { return e.key == 'r' ? (widget_keys++, Handled::yes) : Handled::no; });
  f.app->commands().define("reload", [&] { commands++; });
  f.app->keymap().bind("r", "reload");
  press(*f.app, 'r');
  CHECK(widget_keys == 1 && commands == 0);
  c.disconnect();
  press(*f.app, 'r');
  CHECK(commands == 1);
}

TEST(application_palette_lists_the_commands_takes_every_key_and_escape_closes_it_at_once) {
  Fixture f(20, 60);
  Widget a(f.app->root()), b(f.app->root());
  a.setFocusable(true);
  b.setFocusable(true);
  a.attach(f.app->widgets());
  b.attach(f.app->widgets());
  f.app->input().setFocus(&a);
  int quits = 0;
  f.app->commands().define("quit", "Leave", [&] { quits++; });
  f.app->keymap().bind("q", "quit");
  press(*f.app, 'x', kModAlt);
  CHECK(f.app->commandPaletteOpen());
  CHECK(f.backend->text().find("command-palette") != std::string::npos);
  press(*f.app, 'q');  // typed into the query, not a quit
  CHECK(quits == 0 && f.app->commandPalette().query() == "q");
  press(*f.app, Key::Tab);  // completes in the palette instead of moving the focus
  CHECK(f.app->input().focused() == &a);
  CHECK(f.app->commandPalette().query() == "quit");
  // A bare Escape is held on a terminal in case it starts a Meta key; the palette takes it at once.
  press(*f.app, Key::Escape);
  CHECK(!f.app->commandPaletteOpen() && quits == 0);
  press(*f.app, 'x', kModAlt);
  press(*f.app, 'q');
  press(*f.app, Key::Enter);
  CHECK(!f.app->commandPaletteOpen() && quits == 1);
  press(*f.app, Key::Tab);  // with the palette closed, Tab moves the focus
  CHECK(f.app->input().focused() == &b);
}

TEST(application_run_returns_the_status_given_to_quit) {
  Fixture f;
  f.app->commands().define("fail", [&] { f.app->quit(3); });
  f.app->keymap().bind("q", "fail");
  f.backend->pushKey('q');
  CHECK(f.app->run() == 3);
}

TEST(application_reads_theme_in_both_spellings_and_wants_its_name) {
  std::string error;
  char prog[] = "prog", theme[] = "--theme";
  char * argv[] = {prog, theme};
  CHECK(!Application::open(2, argv, &error) && error.find("--theme needs a name") != std::string::npos);
  char bad[] = "--theme=nonesuch";
  char * argv2[] = {prog, bad};
  CHECK(!Application::open(2, argv2, &error) && error.find("unknown --theme 'nonesuch'") != std::string::npos);
}

TEST(application_palette_lists_runs_and_names_the_programs_own_commands) {
  Fixture f(20, 60);
  std::vector<std::string> ran;
  Application::CommandSource source;
  source.names = [] { return std::vector<std::string>{"play", "quit"}; };
  source.description = [](const std::string & name) { return name == "play" ? "Start playing" : ""; };
  source.binding = [](const std::string & name) { return name == "play" ? "SPC" : ""; };
  source.execute = [&](const std::string & name) {
    ran.push_back(name);
    return name == "play";
  };
  f.app->setCommandSource(std::move(source));
  f.app->commands().define("quit", "Leave", [&] { ran.push_back("own quit"); });
  f.app->keymap().bind("C-q", "quit");
  press(*f.app, 'x', kModAlt);
  std::string text = f.backend->text();
  CHECK(text.find("play") != std::string::npos && text.find("SPC") != std::string::npos);
  CHECK(text.find("command-palette") != std::string::npos);
  for (char c : std::string("play")) press(*f.app, c);
  CHECK(text.find("Start playing") == std::string::npos);  // the description shows for the chosen one
  CHECK(f.backend->text().find("Start playing") != std::string::npos);
  press(*f.app, Key::Enter);
  CHECK(ran == std::vector<std::string>{"play"});
  // commands() and keymap() have it first: one name, its own command and binding.
  CHECK(f.app->bindingFor("quit") == "C-q");
  CHECK(f.app->execute("quit") && ran.back() == "own quit");
  CHECK(!f.app->execute("nonesuch"));
  CHECK(ran.back() == "nonesuch");
}

TEST(application_loop_runs_the_programs_turn_hooks_and_draws_only_what_its_frame_hook_shows) {
  Fixture f;
  int befores = 0, afters = 0, hooked_frames = 0;
  bool show = false;
  f.app->setBeforeTurn([&] {
    befores++;
    return 5;  // a short wait, so turns keep coming without input
  });
  f.app->setAfterTurn([&](bool) {
    if (++afters == 3) {
      show = true;
      f.app->requestFrame();
    }
    if (afters == 6) f.app->quit(7);
  });
  f.app->setFrameHook([&] {
    hooked_frames++;
    return show;
  });
  CHECK(f.app->run() == 7);
  CHECK(befores >= 6 && afters == 6);
  CHECK(hooked_frames >= 2);  // the first frame, held back, and the one asked for after
  CHECK(f.backend->frameCount() >= 1u && f.backend->frameCount() < static_cast<size_t>(hooked_frames));
}

TEST(application_loop_asks_no_frame_for_a_wake_with_nothing_from_the_display) {
  Fixture f;
  int frames = 0, turns = 0;
  f.app->setFrameHook([&] {
    frames++;
    return true;
  });
  f.app->setBeforeTurn([] { return 5; });
  f.app->setAfterTurn([&](bool) {
    turns++;
    if (turns == 2) f.backend->wake();         // another thread's post
    if (turns == 4) f.backend->pushKey('a');  // an input
    if (turns == 7) f.app->quit();
  });
  f.app->run();
  CHECK(frames == 2);  // the first one and the input's, not the wake's
}

TEST(application_keymap_where_the_program_offers_it_runs_ahead_of_the_focused_widget) {
  Fixture f;
  Widget w(f.app->root());
  w.setFocusable(true);
  w.attach(f.app->widgets());
  f.app->input().setFocus(&w);
  std::vector<std::string> ran;
  int widget_keys = 0;
  auto own = w.on<KeyDown>([&](const KeyDown &, EventContext &) {
    widget_keys++;
    return Handled::yes;
  });
  f.app->setKeymapOrder(Application::KeymapOrder::PROGRAM);
  Application::CommandSource source;
  source.execute = [&](const std::string & name) { return ran.push_back("palette " + name), true; };
  source.run_binding = [&](const std::string & name) { return ran.push_back("key " + name), true; };
  source.binding = [](const std::string & name) { return name == "save" ? std::string("F2") : std::string(); };
  f.app->setCommandSource(std::move(source));
  f.app->keymap().bind("C-s", "save");
  f.app->keymap().bind("C-x C-c", "quit");
  // The program's layer on the root, ahead of the focused widget.
  bool layer_open = true;
  auto layer = f.app->widgets().on<KeyDown>([&](const KeyDown & e, EventContext &) {
    return layer_open && f.app->offerKey(e) ? Handled::yes : Handled::no;
  }, Phase::capture);
  press(*f.app, 's', kModCtrl);
  CHECK(ran == std::vector<std::string>{"key save"} && widget_keys == 0);
  // A prefix takes the key after it whatever has the focus, and says so.
  std::vector<std::string> said;
  auto m = f.app->message.connect([&](std::string s) { said.push_back(std::move(s)); });
  press(*f.app, 'x', kModCtrl);
  CHECK(!said.empty() && said.back() == "C-x-");
  layer_open = false;
  press(*f.app, 'c', kModCtrl);
  CHECK(ran.back() == "key quit" && widget_keys == 0);
  // Not offered: the application's own bubble handler leaves the keymap alone, even for a key nothing took.
  own.disconnect();
  press(*f.app, 's', kModCtrl);
  CHECK(ran.size() == 2u);
  // The palette runs a name through execute(); the source's binding comes before the keymap's.
  CHECK(f.app->execute("save") && ran.back() == "palette save");
  CHECK(f.app->bindingFor("save") == "F2" && f.app->bindingFor("quit") == "C-x C-c");
}

TEST(application_a_bare_escape_reaches_an_open_modal_at_once) {
  Fixture f;
  int result = -2;
  f.app->commands().define("info", "Show a box", [&] {
    InfoModal box(f.app->widgets(), f.app->root(), f.app->backend().eventLoop(), "Title", "text");
    result = box.showModal();
  });
  f.app->keymap().bind("i", "info");
  f.app->backend().eventLoop().setBefore([] { return 0; });
  int turn = 0;
  auto frame = f.app->backend().eventLoop().onFrame([&] {
    if (turn++ == 0) press(*f.app, Key::Escape);  // not held for a key that may follow it
    else f.app->backend().eventLoop().quit();
  });
  press(*f.app, 'i');
  CHECK(result == 0);  // closed by the Escape (done), not by the loop ending
}
