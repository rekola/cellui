#include "TestFramework.h"

#include <cellui/Application.h>
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
