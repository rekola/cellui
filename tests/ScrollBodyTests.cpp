#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/ScrollBody.h>

using namespace cellui;

namespace {

// A 20 x 40 screen with a box at row 2, column 5; a body of 4 lines from its row 3, 29 columns wide, so the
// scrollbar is on screen column 34, rows 5-8.
struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{20, 40};
  BufferPlane root{buffer, styles};
  std::unique_ptr<Plane> box = root.createChild();
  ScrollBody body;
  Fixture() {
    box->resize(10, 30);
    box->move(2, 5);
    body.setCount(50);
    layout();
  }
  void layout() {
    const Color c = Color::srgb(0.5f, 0.5f, 0.5f);
    body.layout(root, *box, 2, 5, 3, 4, 29, c, c, c, c);
  }
};

}  // namespace

TEST(scrollbody_scrollbar_drag_follows_the_pointer_and_stops_at_the_end) {
  Fixture f;
  CHECK(f.body.firstLine() == 0);
  CHECK(f.body.press(8, 34));  // the bare track at its bottom: the thumb jumps there
  CHECK(f.body.dragging());
  f.layout();
  CHECK(f.body.firstLine() == 46);
  CHECK(f.body.drag(5, 34));
  f.layout();
  CHECK(f.body.firstLine() == 0);
  CHECK(f.body.release(5, 34));
  CHECK(!f.body.dragging());
}

TEST(scrollbody_press_off_the_scrollbar_is_not_taken) {
  Fixture f;
  CHECK(!f.body.press(8, 10));
  CHECK(!f.body.drag(8, 10));
  CHECK(!f.body.release(8, 10));
}

TEST(scrollbody_has_no_scrollbar_when_everything_fits) {
  Fixture f;
  f.body.setCount(3);
  f.layout();
  CHECK(!f.body.press(5, 34));
}

TEST(scrollbody_keys_scroll_by_lines_and_to_the_ends) {
  Fixture f;
  f.body.scrollBy(7);
  f.layout();
  CHECK(f.body.firstLine() == 7);
  f.body.end();
  f.layout();
  CHECK(f.body.firstLine() == 46);
  f.body.home();
  f.layout();
  CHECK(f.body.firstLine() == 0);
  CHECK(f.body.page() == 3);
}
