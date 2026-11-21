#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/ChoiceDialog.h>
#include <cellui/InputDispatcher.h>

#include <algorithm>
#include <chrono>
#include <thread>

using namespace cellui;

namespace {

KeyDown key(int id, bool ctrl = false) {
  return InputDispatcher::keyDownFor(Input(id, 0, 0, false, false, ctrl, false, Input::Kind::PRESS));
}

// A 10 x 40 screen with a three-entry dialog on it: 8 rows by 32 columns,
// at row 1, column 4; the entries are on rows 3-5.
struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{10, 40};
  BufferPlane root{buffer, styles};
  ChoiceDialog dialog;
  Fixture() {
    dialog.open("Pick", {"alpha", "beta", "gamma"}, 0);
    dialog.show(root, 10, 40);
    root.refresh();
  }
};

}  // namespace

TEST(choicedialog_draws_title_entries_marker_and_hint_without_a_border) {
  Fixture f;
  CHECK(f.dialog.isOpen());
  CHECK(f.buffer.toPlainText() ==
        "\n\n"
        "      Pick\n"
        "      ● alpha\n"
        "        beta\n"
        "        gamma\n"
        "\n"
        "      Enter: choose  Esc: cancel\n");
  CHECK(f.buffer.toPlainText().find("│") == std::string::npos);
}

TEST(choicedialog_title_is_bold) {
  Fixture f;
  CHECK(f.buffer.at(2, 6).bold());
  CHECK(!f.buffer.at(3, 6).bold());
}

TEST(choicedialog_keys_move_choose_and_cancel) {
  Fixture f;
  using R = ChoiceDialog::Result;
  CHECK(f.dialog.key(key(cellui::Key::Down)) == R::NONE);
  CHECK(f.dialog.selected() == 1);
  CHECK(f.dialog.key(key('n', true)) == R::NONE);
  CHECK(f.dialog.selected() == 2);
  CHECK(f.dialog.key(key(cellui::Key::Down)) == R::NONE);
  CHECK(f.dialog.selected() == 2);  // stops at the end
  CHECK(f.dialog.key(key(cellui::Key::Home)) == R::NONE);
  CHECK(f.dialog.selected() == 0);
  CHECK(f.dialog.key(key(cellui::Key::End)) == R::NONE);
  CHECK(f.dialog.selected() == 2);
  CHECK(f.dialog.key(key(cellui::Key::Enter)) == R::CHOSEN);
  CHECK(f.dialog.key(key(cellui::Key::Escape)) == R::CANCELLED);
  CHECK(f.dialog.key(key('q')) == R::CANCELLED);
  CHECK(f.dialog.key(key('g', true)) == R::CANCELLED);
}

TEST(choicedialog_click_on_an_entry_chooses_it_outside_cancels_padding_is_inert) {
  Fixture f;
  using R = ChoiceDialog::Result;
  CHECK(f.dialog.click(4, 10) == R::CHOSEN);  // second entry
  CHECK(f.dialog.selected() == 1);
  CHECK(f.dialog.click(0, 0) == R::CANCELLED);
  CHECK(f.dialog.click(2, 10) == R::NONE);  // the title
}

TEST(choicedialog_long_lists_scroll_and_show_a_scrollbar_on_the_edge) {
  StyleProvider styles;
  CellBuffer buffer(10, 40);
  BufferPlane root(buffer, styles);
  ChoiceDialog dialog;
  std::vector<std::string> labels;
  for (int i = 1; i <= 10; i++) labels.push_back("item " + std::to_string(i));
  dialog.open("Many", labels, 0);
  dialog.show(root, 10, 40);  // room for 3 entries
  root.refresh();
  CHECK(buffer.toPlainText().find("item 3") != std::string::npos);
  CHECK(buffer.toPlainText().find("item 4") == std::string::npos);
  auto thumb_row = [&] {
    auto text = buffer.toPlainText();
    int row = 0;
    for (size_t start = 0, end; (end = text.find('\n', start)) != std::string::npos || start < text.size(); start = end + 1, row++) {
      if (text.substr(start, end == std::string::npos ? end : end - start).find("┃") != std::string::npos) return row;
      if (end == std::string::npos) break;
    }
    return -1;
  };
  CHECK(buffer.toPlainText().find("│") != std::string::npos);
  int top = thumb_row();
  CHECK(top >= 0);
  dialog.key(key(cellui::Key::End));
  dialog.show(root, 10, 40);
  root.refresh();
  CHECK(buffer.toPlainText().find("item 10") != std::string::npos);
  CHECK(thumb_row() > top);  // the thumb has moved to the end of the track
}

TEST(choicedialog_long_label_ends_in_an_ellipsis) {
  StyleProvider styles;
  CellBuffer buffer(8, 20);
  BufferPlane root(buffer, styles);
  ChoiceDialog dialog;
  dialog.open("T", {"a label far wider than the screen allows"}, -1);
  dialog.show(root, 8, 20);
  root.refresh();
  CHECK(buffer.toPlainText().find("\xe2\x80\xa6") != std::string::npos);
}

TEST(choicedialog_close_removes_it_and_empty_dialog_cancels) {
  Fixture f;
  f.dialog.close();
  f.root.refresh();
  CHECK(!f.dialog.isOpen());
  CHECK(f.buffer.toPlainText().empty());
  CHECK(f.dialog.key(key('x')) == ChoiceDialog::Result::CANCELLED);
}

TEST(choicedialog_smooth_list_scrolls_under_a_cursor_band_that_stays_at_the_edge) {
  StyleProvider styles;
  CellBuffer buffer(10, 40);
  BufferPlane root(buffer, styles);
  root.setSmoothScroll(true);
  ChoiceDialog dialog;
  std::vector<std::string> labels;
  for (int i = 1; i <= 20; i++) labels.push_back("item " + std::to_string(i));
  dialog.open("Many", labels, 0);  // room for 3 entries
  auto show = [&]() {
    dialog.show(root, 10, 40);
    root.refresh();
    return root.takeBackdropLayers();
  };
  auto band_row = [&](const std::vector<BackdropLayer> & layers) {
    for (auto & layer : layers) {
      if (layer.scroll && layer.scroll->band) return layer.scroll->band->row;
    }
    return -1.0;
  };
  show();
  // Down past the bottom of the window: the band reaches the last row and stays there while the list scrolls.
  for (int i = 0; i < 6; i++) dialog.key(key(cellui::Key::Down));
  bool shifted = false;
  double edge = -1;
  for (int i = 0; i < 100; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    auto layers = show();
    for (auto & layer : layers) {
      if (layer.scroll && layer.scroll->shift_rows > 0 && layer.scroll->shift_rows < 1) shifted = true;
    }
    if (!dialog.animating()) edge = band_row(layers);
  }
  CHECK(shifted);
  CHECK(!dialog.animating());
  CHECK(edge == 2.0);  // the last of the three rows
  CHECK(dialog.selected() == 6);
}

TEST(choicedialog_labels_are_proportional_text_where_the_display_lays_text_out) {
  StyleProvider styles;
  CellBuffer buffer(10, 40);
  BufferPlane root(buffer, styles);
  root.setProportionalText(true);
  ChoiceDialog dialog;
  dialog.open("Pick", {"First", "Second"}, 1);
  dialog.show(root, 10, 40);
  root.refresh();
  std::vector<std::string> texts;
  for (auto & run : root.takeTextRuns()) texts.push_back(run.text);
  CHECK(std::find(texts.begin(), texts.end(), "First") != texts.end() && std::find(texts.begin(), texts.end(), "Second") != texts.end());
  CHECK(buffer.toPlainText().find("●") != std::string::npos);  // the marker stays in a cell
}

TEST(choicedialog_wheel_scrolls_without_moving_the_selection) {
  StyleProvider styles;
  CellBuffer buffer(10, 40);
  BufferPlane root(buffer, styles);
  ChoiceDialog dialog;
  std::vector<std::string> labels;
  for (int i = 1; i <= 10; i++) labels.push_back("item " + std::to_string(i));
  dialog.open("Many", labels, 0);
  dialog.show(root, 10, 40);
  for (int i = 0; i < 20; i++) dialog.wheel(false);
  dialog.show(root, 10, 40);
  root.refresh();
  CHECK(dialog.selected() == 0);
  CHECK(buffer.toPlainText().find("item 10") != std::string::npos);
  CHECK(buffer.toPlainText().find("item 1\n") == std::string::npos);
  dialog.key(key(cellui::Key::Down));
  CHECK(dialog.selected() == 1);
}

TEST(choicedialog_chooses_the_entry_the_button_is_released_over_after_a_press_on_an_entry) {
  Fixture f;
  using R = ChoiceDialog::Result;
  CHECK(f.dialog.press(3, 10) == R::NONE);  // alpha
  CHECK(f.dialog.drag(5, 10) == R::NONE);
  CHECK(f.dialog.click(5, 10) == R::CHOSEN);  // gamma
  CHECK(f.dialog.selected() == 2);
}

TEST(choicedialog_press_on_an_entry_released_elsewhere_chooses_nothing) {
  Fixture f;
  using R = ChoiceDialog::Result;
  f.dialog.press(3, 10);
  CHECK(f.dialog.click(2, 10) == R::NONE);  // the title
  CHECK(f.dialog.selected() == 0);
  Fixture g;
  g.dialog.press(0, 0);
  CHECK(g.dialog.click(0, 0) == R::CANCELLED);
}
