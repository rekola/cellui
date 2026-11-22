#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/Button.h>
#include <cellui/Utf8.h>

using namespace cellui;

namespace {

struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{3, 20};
  BufferPlane root{buffer, styles};
  Button button{root, ButtonFace::info()};
  int clicks = 0;
  Connection connection = button.clicked.connect([this] { clicks++; });
  Fixture() {
    button.resize(1, button.preferredWidth());
    button.move(1, 4);
    root.refresh();
  }
  void press(int row, int col) { button.post(MouseDown{static_cast<float>(col), static_cast<float>(row), row, col, 1, {}}); }
  void release(int row, int col) { button.post(MouseUp{static_cast<float>(col), static_cast<float>(row), row, col, 1, {}}); }
  void key(int k) {
    KeyDown e;
    e.key = k;
    button.post(e);
  }
};

}  // namespace

TEST(button_draws_its_face_and_the_info_face_is_the_circled_information_source) {
  Fixture f;
  f.root.refresh();
  CHECK(f.buffer.toPlainText().find("\xF0\x9F\x9B\x88") != std::string::npos);
  CHECK(std::string(kInfoGlyph) == "\xF0\x9F\x9B\x88");  // U+1F6C8
  CHECK(f.button.preferredWidth() == utf8::displayWidth(kInfoGlyph));
  f.button.setFace("[x]");
  f.button.resize(1, f.button.preferredWidth());
  f.root.refresh();
  CHECK(f.buffer.toPlainText().find("[x]") != std::string::npos);
}

TEST(button_click_is_a_press_and_a_release_on_it) {
  Fixture f;
  f.press(1, 4);
  CHECK(f.button.pressed() && f.clicks == 0);
  f.release(1, 4);
  CHECK(!f.button.pressed() && f.clicks == 1);
  f.press(1, 4);
  f.release(2, 9);  // let go elsewhere: not a click
  CHECK(!f.button.pressed() && f.clicks == 1);
  f.release(1, 4);  // a release with no press
  CHECK(f.clicks == 1);
}

TEST(button_moving_off_it_unpresses_and_moving_back_presses_again) {
  Fixture f;
  f.press(1, 4);
  f.button.post(MouseMove{9, 2, 2, 9, 1, {}});
  CHECK(!f.button.pressed());
  f.button.post(MouseMove{4, 1, 1, 4, 1, {}});
  CHECK(f.button.pressed());
  f.release(1, 4);
  CHECK(f.clicks == 1);
}

TEST(button_space_and_enter_click_it_other_keys_do_not) {
  Fixture f;
  f.key(Key::Space);
  f.key(Key::Enter);
  CHECK(f.clicks == 2);
  f.key('x');
  CHECK(f.clicks == 2);
}
