#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/Checkbox.h>
#include <cellui/InputDispatcher.h>

using namespace cellui;

namespace {

struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{2, 30};
  BufferPlane root{buffer, styles};
  bool value = false;
  Checkbox box{root, "Borrowed chords", [this] { return value; }, [this](bool v) { value = v; }};
  Fixture() {
    box.resize(1, box.preferredWidth());
    box.move(1, 2);
    box.render(styles, true);
    root.refresh();
  }
};

KeyDown keyDown(int key, bool shift = false) {
  KeyDown e;
  e.key = key;
  e.mods.shift = shift;
  return e;
}

}  // namespace

TEST(checkbox_shows_its_state_and_label) {
  Fixture f;
  CHECK(f.buffer.toPlainText() == "\n  [ ] Borrowed chords\n");
  f.value = true;
  CHECK(f.box.render(f.styles));
  f.root.refresh();
  CHECK(f.buffer.toPlainText() == "\n  [x] Borrowed chords\n");
  CHECK(f.box.preferredWidth() == 4 + 15);
}

TEST(checkbox_render_only_redraws_when_something_changed) {
  Fixture f;
  CHECK(!f.box.render(f.styles));
  f.box.setFocused(true);
  CHECK(f.box.render(f.styles));
  CHECK(!f.box.render(f.styles));
  f.value = true;
  CHECK(f.box.render(f.styles));
}

TEST(checkbox_space_and_enter_toggle_it_and_other_keys_do_not) {
  Fixture f;
  CHECK(f.box.key(keyDown(Key::Space)));
  CHECK(f.value);
  CHECK(f.box.key(keyDown(Key::Enter)));
  CHECK(!f.value);
  CHECK(!f.box.key(keyDown('a')));
  CHECK(!f.box.key(keyDown(Key::Space, true)));
  CHECK(!f.value);
  auto repeat = keyDown(Key::Space);
  repeat.repeat = true;
  CHECK(f.box.key(repeat));
  CHECK(!f.value);
}

TEST(checkbox_takes_the_focus_and_is_toggled_through_the_dispatcher) {
  Fixture f;
  CHECK(f.box.focusable());
  Widget top(f.root);
  f.box.attach(top);
  InputDispatcher dispatcher(top);
  dispatcher.setFocus(&f.box);
  CHECK(f.box.focused());
  dispatcher.keyDown(keyDown(Key::Space));
  CHECK(f.value);
  dispatcher.setFocus(nullptr);
  CHECK(!f.box.focused());
}
