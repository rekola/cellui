#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/Label.h>
#include <cellui/Layout.h>
#include <cellui/Theme.h>

using namespace cellui;

namespace {

struct Screen {
  StyleProvider styles;
  CellBuffer buffer;
  BufferPlane root;
  Screen(int rows, int cols) : buffer(rows, cols), root(buffer, styles) {}
  std::string text() {
    root.refresh();
    return buffer.toPlainText();
  }
};

bool placed(const Widget & w, int row, int col, int rows, int cols) {
  return w.getPosition() == std::pair(row, col) && w.getDim() == std::pair(rows, cols);
}

}  // namespace

TEST(layout_rows_gives_sized_parts_their_rows_and_shares_the_rest) {
  Screen s(20, 40);
  Widget top(s.root), middle(s.root), bottom(s.root), a(s.root), b(s.root);
  layoutRows(0, 0, 20, 40, {{.widget = &top, .margin_start = 1}, {.size = 1}, {.widget = &middle, .fill = 1}, {.widget = &bottom, .size = 2}});
  CHECK(placed(top, 0, 1, 1, 39));
  CHECK(placed(middle, 2, 0, 16, 40));
  CHECK(placed(bottom, 18, 0, 2, 40));
  // Shares of what is left, the last filling part taking the remainder.
  layoutRows(1, 2, 10, 20, {{.widget = &a, .fill = 1}, {.widget = &b, .fill = 2}});
  CHECK(placed(a, 1, 2, 3, 20));
  CHECK(placed(b, 4, 2, 7, 20));
}

TEST(layout_rows_cuts_off_what_does_not_fit) {
  Screen s(3, 10);
  Widget a(s.root), fill(s.root), b(s.root), c(s.root);
  layoutRows(0, 0, 3, 10, {{.widget = &a, .size = 2}, {.widget = &fill, .fill = 1}, {.widget = &b, .size = 2}, {.widget = &c}});
  CHECK(placed(a, 0, 0, 2, 10));
  CHECK(fill.getDim().first == 0);
  CHECK(b.getDim().first == 1);  // what is left of the rectangle
  CHECK(c.getDim().first == 0);
}

TEST(layout_columns_lays_out_left_to_right) {
  Screen s(5, 30);
  Widget left(s.root), right(s.root);
  layoutColumns(0, 0, 5, 30, {{.widget = &left, .size = 10, .margin_start = 1, .margin_end = 1}, {.widget = &right, .fill = 1}});
  CHECK(placed(left, 1, 0, 3, 10));
  CHECK(placed(right, 0, 10, 5, 20));
}

TEST(label_shows_its_text_on_its_background_and_follows_the_theme) {
  Screen s(3, 40);
  Label label(s.root);
  label.resize(1, 20);
  label.move(1, 0);
  label.setStyle({TextFont::MONOSPACE, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::LEFT});
  label.setText("stocks - made-up data");
  std::string t = s.text();
  CHECK(t.find("stocks - made-up da") != std::string::npos);  // cut with an ellipsis to its 20 cells
  CHECK(t.find("data") == std::string::npos);
  label.setColors(&StyleProvider::info_line_fg_color, &StyleProvider::info_line_bg_color);
  s.root.refresh();
  CHECK(s.buffer.at(1, 19).bg == s.styles.info_line_bg_color);  // the background fills the row
  s.styles.applyTheme(Theme::light());
  label.render();
  s.root.refresh();
  CHECK(s.buffer.at(1, 19).bg == s.styles.info_line_bg_color);
  label.resize(1, 30);  // a resize draws it again
  CHECK(s.text().find("stocks - made-up data") != std::string::npos);
}
