#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/Table.h>
#include <cellui/TagBar.h>

using namespace cellui;

namespace {

struct Quotes : TableModel {
  struct Row {
    std::string name, price;
    std::vector<float> history;
  };
  std::vector<Row> rows = {{"Nokia", "4.20", {1, 2, 3}}, {"Fortum", "15.50", {3, 2, 1}}, {"Sampo", "9.00", {1, 3, 2}}, {"Kone", "52.10", {1, 1, 4}}};
  int rowCount() const override { return static_cast<int>(rows.size()); }
  std::string text(int r, int c) const override { return c == 0 ? rows[static_cast<size_t>(r)].name : rows[static_cast<size_t>(r)].price; }
  std::vector<float> trace(int r, int) const override { return rows[static_cast<size_t>(r)].history; }
};

struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{8, 30};
  BufferPlane root{buffer, styles};
  std::unique_ptr<Charts> charts = cellCharts();
  Table table{root, *charts};
  std::shared_ptr<Quotes> model = std::make_shared<Quotes>();
  Fixture() {
    table.setColumns({{"Name", 10, Align::LEFT, true, false}, {"Price", 8, Align::RIGHT, true, false}, {"Trend", 6, Align::LEFT, false, true}});
    table.resize(6, 30);
    table.move(1, 0);
    table.setModel(model);
    root.refresh();
  }
  std::string text() {
    root.refresh();
    return buffer.toPlainText();
  }
  void press(int row, int col) { table.post(MouseDown{static_cast<float>(col), static_cast<float>(row), row, col, 1, {}}); }
  void release(int row, int col) { table.post(MouseUp{static_cast<float>(col), static_cast<float>(row), row, col, 1, {}}); }
  void click(int row, int col) {
    press(row, col);
    release(row, col);
  }
  void keyPress(int key) {
    KeyDown k;
    k.key = key;
    table.post(k);
  }
};

}  // namespace

TEST(table_draws_heading_rows_and_right_aligned_numbers) {
  Fixture f;
  std::string t = f.text();
  CHECK(t.find("Name") != std::string::npos);
  CHECK(t.find("Nokia") != std::string::npos);
  CHECK(t.find("      4.20") != std::string::npos);
}

TEST(table_heading_click_sorts_ascending_descending_then_back) {
  Fixture f;
  f.click(1, 12);  // Price heading: columns are at 0, 11, 20
  CHECK(f.table.sortColumn() == 1 && f.table.sortOrder() == SortOrder::ASCENDING);
  CHECK(f.table.rowAt(0) == 0);  // 4.20 is lowest; compared as numbers, 52.10 > 9.00
  CHECK(f.table.rowAt(3) == 3);
  CHECK(f.text().find("Price ▲") != std::string::npos);
  f.click(1, 12);
  CHECK(f.table.sortOrder() == SortOrder::DESCENDING);
  CHECK(f.table.rowAt(0) == 3);
  f.click(1, 12);
  CHECK(f.table.sortOrder() == SortOrder::NONE);
  CHECK(f.table.rowAt(0) == 0 && f.table.rowAt(1) == 1);
  f.click(1, 25);  // the trace column is not sortable
  CHECK(f.table.sortOrder() == SortOrder::NONE);
}

TEST(table_selection_keys_clicks_and_signals) {
  Fixture f;
  int selected = -2, activated = -2;
  auto a = f.table.selection_changed.connect([&](int r) { selected = r; });
  auto b = f.table.activated.connect([&](int r) { activated = r; });
  f.keyPress(Key::Down);
  CHECK(selected == 0);
  f.keyPress(Key::End);
  CHECK(selected == 3);
  f.keyPress(Key::Down);
  CHECK(selected == 3);  // stops at the end
  f.keyPress(Key::Enter);
  CHECK(activated == 3);
  f.click(3, 5);  // second body row on screen: row 1 is the heading's, table sits at 1
  CHECK(selected == 1);  // table row 2 of screen => position 1
  f.keyPress(Key::Home);
  CHECK(selected == 0);
}

TEST(table_selection_follows_its_row_through_a_sort) {
  Fixture f;
  f.table.select(2);  // Sampo
  f.table.sortBy(0, SortOrder::ASCENDING);  // Fortum Kone Nokia Sampo
  CHECK(f.table.selectedRow() == 2);
  CHECK(f.table.selectedPosition() == 3);
}

TEST(table_scrolls_with_the_wheel_and_keeps_the_selection_in_view) {
  Fixture f;
  f.table.resize(3, 30);  // heading and two rows
  CHECK(f.table.visibleRows() == 2);
  f.table.post(Wheel{0, 0, 2, 3, 0, -1, {}});  // one notch down
  CHECK(f.table.firstVisiblePosition() == 2);  // at most content - view
  f.table.post(Wheel{0, 0, 2, 3, 0, 1, {}});
  CHECK(f.table.firstVisiblePosition() == 0);
  f.table.select(3);
  CHECK(f.table.firstVisiblePosition() == 2);
}

TEST(table_trace_column_draws_braille_dots) {
  Fixture f;
  std::string t = f.text();
  // A rising series has dots, none of them blank-braille.
  CHECK(t.find("\xe2\xa0") != std::string::npos || t.find("\xe2\xa1") != std::string::npos || t.find("\xe2\xa3") != std::string::npos);
}

TEST(table_keys_cycle_the_sort_column_and_reverse) {
  Fixture f;
  KeyDown k;
  k.key = '>';
  f.table.post(k);
  CHECK(f.table.sortColumn() == 0);
  f.table.post(k);
  CHECK(f.table.sortColumn() == 1);
  f.table.post(k);  // the trace column is skipped
  CHECK(f.table.sortColumn() == 0);
  k.key = 'r';
  f.table.post(k);
  CHECK(f.table.sortOrder() == SortOrder::DESCENDING);
}

TEST(tagbar_click_and_arrows_choose) {
  StyleProvider styles;
  CellBuffer buffer(2, 40);
  BufferPlane root(buffer, styles);
  TagBar bar(root);
  bar.resize(1, 40);
  bar.setTags({"Commodities", "Nasdaq Helsinki", "FX"});
  int chosen = -2;
  auto c = bar.chosen.connect([&](int i) { chosen = i; });
  CHECK(bar.tagAt(0) == 0);
  CHECK(bar.tagAt(13) == -1);  // the gap
  CHECK(bar.tagAt(14) == 1);
  bar.post(MouseDown{15, 0, 0, 15, 1, {}});
  CHECK(chosen == 1 && bar.selected() == 1);
  KeyDown k;
  k.key = Key::Right;
  bar.post(k);
  CHECK(bar.selected() == 2);
  bar.post(k);
  CHECK(bar.selected() == 2);
  root.refresh();
  CHECK(buffer.toPlainText().find(" Nasdaq Helsinki ") != std::string::npos);
}

TEST(trace_resample_scales_lowest_to_highest) {
  auto v = trace_image::resample({10, 20, 30}, 5);
  CHECK(v.size() == 5u);
  CHECK(v.front() == 0.0f && v.back() == 1.0f);
  CHECK(v[2] == 0.5f);
  CHECK(trace_image::resample({}, 4).empty());
  auto flat = trace_image::resample({7, 7}, 3);
  CHECK(flat[0] == 0.5f);
}

TEST(table_row_is_pressed_while_the_button_is_held_and_selected_on_release) {
  Fixture f;
  int selected = -2;
  auto c = f.table.selection_changed.connect([&](int r) { selected = r; });
  f.keyPress(Key::Home);
  CHECK(selected == 0);
  f.press(4, 5);  // the third row on screen: position 2
  f.root.refresh();
  CHECK(f.buffer.at(4, 5).bg == f.styles.row_pressed_bg_color);
  CHECK(f.table.selectedRow() == 0);  // nothing chosen until the release
  f.release(4, 5);
  f.root.refresh();
  CHECK(f.table.selectedRow() == 2);
  CHECK(f.buffer.at(4, 5).bg != f.styles.row_pressed_bg_color);
}

TEST(table_pressed_look_moves_with_the_pointer_and_release_selects_the_row_it_is_on) {
  Fixture f;
  f.press(4, 5);  // position 2
  f.table.post(MouseMove{6, 5, 5, 6, 1u, {}});  // dragged to the next row, button held
  f.root.refresh();
  CHECK(f.buffer.at(4, 5).bg != f.styles.row_pressed_bg_color);
  CHECK(f.buffer.at(5, 6).bg == f.styles.row_pressed_bg_color);
  CHECK(f.table.selectedRow() == -1);  // nothing chosen until the release
  f.release(5, 6);
  CHECK(f.table.selectedRow() == 3);
  f.root.refresh();
  CHECK(f.buffer.at(5, 6).bg != f.styles.row_pressed_bg_color);
}

TEST(table_press_dragged_off_the_rows_shows_nothing_pressed_and_release_selects_nothing) {
  Fixture f;
  f.press(4, 5);
  f.table.post(MouseMove{5, 0, 0, 5, 1u, {}});  // over the tag row, above the table
  f.root.refresh();
  CHECK(f.buffer.at(4, 5).bg != f.styles.row_pressed_bg_color);
  f.release(0, 5);
  CHECK(f.table.selectedRow() == -1);
  f.table.post(MouseMove{5, 4, 4, 5, 1u, {}});  // no button held now: nothing follows
  f.root.refresh();
  CHECK(f.buffer.at(4, 5).bg != f.styles.row_pressed_bg_color);
}

TEST(table_pressed_heading_follows_the_pointer_among_the_headings_and_sorts_by_the_one_released_on) {
  Fixture f;
  f.press(1, 2);  // Name heading (column 0)
  f.table.post(MouseMove{12, 1, 1, 12, 1u, {}});  // dragged to the Price heading
  f.root.refresh();
  CHECK(f.buffer.at(1, 2).bg != f.styles.button_pressed_bg_color);
  CHECK(f.buffer.at(1, 12).bg == f.styles.button_pressed_bg_color);
  CHECK(f.table.sortOrder() == SortOrder::NONE);
  f.release(1, 12);
  CHECK(f.table.sortColumn() == 1 && f.table.sortOrder() == SortOrder::ASCENDING);
}

TEST(table_heading_is_pressed_while_held_and_sorts_on_release) {
  Fixture f;
  f.press(1, 12);
  f.root.refresh();
  CHECK(f.buffer.at(1, 12).bg == f.styles.button_pressed_bg_color);
  CHECK(f.table.sortOrder() == SortOrder::NONE);
  f.release(1, 12);
  CHECK(f.table.sortColumn() == 1);
}

TEST(trace_line_is_a_gradient_that_is_lit_past_white_only_at_the_top) {
  TraceLook look{Color(10, 10, 10), Color(60, 180, 80), std::nullopt};
  Color low = trace_image::lineColorAt(look, 0.1f, true), high = trace_image::lineColorAt(look, 1.0f, true);
  auto top = std::max({high.r(), high.g(), high.b()});
  CHECK(top > 1.0f);
  CHECK(std::max({low.r(), low.g(), low.b()}) < 1.0f);
  Color flat = trace_image::lineColorAt(look, 1.0f, false);
  CHECK(std::max({flat.r(), flat.g(), flat.b()}) <= 1.0f);
}

TEST(table_text_is_laid_out_by_the_display_when_it_can_and_aligned_in_cells_when_not) {
  Fixture f;  // no proportional text here: the box is written in cells, the numbers right-aligned
  CHECK(f.text().find("      4.20") != std::string::npos);
  CHECK(f.table.font() == TextFont::PROPORTIONAL);
  f.table.setFont(TextFont::MONOSPACE);
  CHECK(f.text().find("      4.20") != std::string::npos);
}

TEST(table_cells_are_text_runs_of_their_column_alignment_on_a_display_with_proportional_text) {
  StyleProvider styles;
  CellBuffer buffer(8, 30);
  BufferPlane root(buffer, styles);
  root.setProportionalText(true);
  std::unique_ptr<Charts> charts = cellCharts();
  Table table(root, *charts);
  table.setColumns({{"Name", 10, Align::LEFT, true, false}, {"Price", 8, Align::RIGHT, true, false}});
  table.resize(4, 30);
  table.setModel(std::make_shared<Quotes>());
  root.refresh();
  auto runs = root.takeTextRuns();
  bool right = false, left = false;
  for (auto & run : runs) {
    if (run.text == "4.20") right = run.align == TextAlign::RIGHT && run.overflow == TextOverflow::ELLIPSIS;
    if (run.text == "Nokia") left = run.align == TextAlign::LEFT;
  }
  CHECK(right && left);
}
