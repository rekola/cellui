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
  std::span<const float> trace(int r, int) const override { return rows[static_cast<size_t>(r)].history; }
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
  CHECK(f.table.firstVisiblePosition() == 1);  // one row for an isolated notch
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

TEST(table_sorts_by_its_headings_and_leaves_letter_keys_to_others) {
  Fixture f;
  for (int key : {'<', '>', 'r'}) {
    KeyDown k;
    k.key = key;
    CHECK(f.table.post(k) == Handled::no);
  }
  CHECK(f.table.sortColumn() == -1);
}

namespace {
// Prices shown rounded to whole euros, and their real values.
struct Rounded : TableModel {
  std::vector<double> prices = {9.6, 9.4, 10.2};
  int rowCount() const override { return static_cast<int>(prices.size()); }
  std::string text(int r, int) const override { return std::to_string(static_cast<int>(prices[static_cast<size_t>(r)] + 0.5)); }
  std::optional<double> number(int r, int) const override { return prices[static_cast<size_t>(r)]; }
};
}  // namespace

TEST(table_sorts_by_the_models_numbers_not_its_rounded_text) {
  Fixture f;
  f.table.setColumns({{"Price", 8, Align::RIGHT, true, false}});
  f.table.setModel(std::make_shared<Rounded>());
  f.table.sortBy(0, SortOrder::ASCENDING);
  CHECK(f.table.rowAt(0) == 1 && f.table.rowAt(1) == 0 && f.table.rowAt(2) == 2);  // 9.4, 9.6, 10.2 although two read "10"
  // The default reads a number from the text: "15.50" after "9.00", not before it.
  f.table.setColumns({{"Name", 10, Align::LEFT, true, false}, {"Price", 8, Align::RIGHT, true, false}});
  f.table.setModel(f.model);
  f.table.sortBy(1, SortOrder::ASCENDING);
  CHECK(f.model->text(f.table.rowAt(1), 1) == "9.00" && f.model->text(f.table.rowAt(2), 1) == "15.50");
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
  auto v = trace_image::resample(std::vector<float>{10, 20, 30}, 5);
  CHECK(v.size() == 5u);
  CHECK(v.front() == 0.0f && v.back() == 1.0f);
  CHECK(v[2] == 0.5f);
  CHECK(trace_image::resample({}, 4).empty());
  auto flat = trace_image::resample(std::vector<float>{7, 7}, 3);
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

TEST(table_drag_past_the_first_or_last_row_presses_that_row_whatever_the_column) {
  Fixture f;
  f.press(4, 5);  // position 2
  f.table.post(MouseMove{25, 0, 0, 25, 1u, {}});  // over the tag row above the table, far to the right
  f.root.refresh();
  CHECK(f.buffer.at(2, 5).bg == f.styles.row_pressed_bg_color);  // the first row
  f.table.post(MouseMove{5, 30, 7, 5, 1u, {}});  // below the table (the model has four rows)
  f.root.refresh();
  CHECK(f.buffer.at(5, 5).bg == f.styles.row_pressed_bg_color);  // the last row
  f.release(7, 5);
  CHECK(f.table.selectedRow() == 3);
  f.table.post(MouseMove{5, 4, 4, 5, 1u, {}});  // no button held now: nothing follows
  f.root.refresh();
  CHECK(f.buffer.at(4, 5).bg != f.styles.row_pressed_bg_color);
}

TEST(table_drag_past_the_bottom_or_top_scrolls_a_row_a_move_and_release_selects_the_row_reached) {
  Fixture f;
  f.table.resize(3, 30);  // the heading and two rows in view, four rows in all
  f.press(2, 5);          // the first row
  CHECK(f.table.firstVisiblePosition() == 0);
  f.table.post(MouseMove{5, 8, 8, 5, 1u, {}});  // below the table
  CHECK(f.table.firstVisiblePosition() == 1);
  f.table.post(MouseMove{5, 9, 9, 5, 1u, {}});
  CHECK(f.table.firstVisiblePosition() == 2);
  f.table.post(MouseMove{5, 9, 9, 5, 1u, {}});  // at the end: it cannot scroll further
  CHECK(f.table.firstVisiblePosition() == 2);
  f.release(9, 5);
  CHECK(f.table.selectedRow() == 3);  // the last row
  f.press(3, 5);
  f.table.post(MouseMove{5, 0, 0, 5, 1u, {}});  // above the table
  CHECK(f.table.firstVisiblePosition() == 1);
  f.release(0, 5);
  CHECK(f.table.selectedRow() == 1);  // the first row in view
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

TEST(tagbar_places_its_chips_in_the_bars_own_columns_in_a_flat_tree_too) {
  StyleProvider styles;
  CellBuffer buffer(3, 40);
  BufferPlane root(buffer, styles);
  root.setFlatChildren(true);
  TagBar bar(root);
  bar.resize(1, 30);
  bar.move(1, 5);
  bar.setTags({"ab", "cd"});
  bar.select(0);
  root.refresh();
  // The bar is at column 5, its first chip " ab " starts there and the click mapping agrees.
  CHECK(buffer.toPlainText().find("\n     " " ab ") != std::string::npos);
  CHECK(bar.tagAt(0) == 0);
}

namespace {
struct SmoothFixture {
  StyleProvider styles;
  CellBuffer buffer{8, 30};
  BufferPlane root{buffer, styles};
  std::unique_ptr<Charts> charts = cellCharts();
  Table table{root, *charts};
  SmoothFixture(bool smooth) {
    root.setSmoothScroll(smooth);
    table.setColumns({{"Name", 10, Align::LEFT, true, false}, {"Price", 8, Align::RIGHT, true, false}});
    table.resize(3, 30);  // the heading and two rows in view, four rows in all
    table.move(1, 0);
    table.setModel(std::make_shared<Quotes>());
    root.refresh();
  }
  void wheelDown() { table.post(Wheel{0, 0, 3, 3, 0, -1, {}}); }
};
}  // namespace

TEST(table_eases_to_a_new_scroll_position_where_the_display_scrolls_smoothly) {
  SmoothFixture f(true);
  CHECK(!f.table.animating());
  f.wheelDown();
  CHECK(f.table.firstVisiblePosition() == 1);  // where it is scrolled to
  CHECK(f.table.animating());
  CHECK(f.table.scrollPosition() == 0.0);      // not there yet
  CHECK(f.table.tick(0.03));                   // still moving
  CHECK(f.table.scrollPosition() > 0.0 && f.table.scrollPosition() < 1.0);
  int ticks = 0;
  while (f.table.tick(0.03) && ticks < 200) ticks++;
  CHECK(ticks < 200);
  CHECK(!f.table.animating());
  CHECK(f.table.scrollPosition() == 1.0);
  f.root.refresh();  // and it draws without trouble in each state
}

TEST(table_jumps_where_the_display_does_not_scroll_smoothly_and_with_taller_rows) {
  SmoothFixture plain(false);
  plain.wheelDown();
  CHECK(!plain.table.animating() && plain.table.scrollPosition() == 1.0);
  SmoothFixture tall(true);
  tall.table.setRowHeight(2);  // whole rows only: the plane would have to be shifted by parts of a table row
  tall.wheelDown();
  CHECK(!tall.table.animating());
}

TEST(table_rows_are_under_the_pointer_by_where_they_are_shown_while_easing) {
  SmoothFixture f(true);
  f.wheelDown();
  f.table.tick(0.03);
  // Halfway into the first row's height the pointer is over what is shown there, not over where the scroll is going.
  f.table.post(MouseDown{5, 2, 2, 5, 1, {}});
  f.table.post(MouseUp{5, 2, 2, 5, 1, {}});
  CHECK(f.table.selectedRow() == 0 || f.table.selectedRow() == 1);  // the first row or the next, never the ones scrolled to
}

namespace {

// A model whose first two rows share one icon id and whose third has none.
struct IconModel : Quotes {
  std::shared_ptr<const TableIcon> shared = std::make_shared<const TableIcon>(
      TableIcon{7, 2, 2, std::make_shared<const std::vector<uint32_t>>(std::vector<uint32_t>(4, Image::pack(200, 30, 30)))});
  std::shared_ptr<const TableIcon> icon(int r, int c) const override { return c == 0 && r < 2 ? shared : nullptr; }
};

struct IconFixture {
  StyleProvider styles;
  CellBuffer buffer{8, 30};
  BufferPlane root{buffer, styles};
  std::unique_ptr<Charts> charts = cellCharts();
  Table table{root, *charts};
  IconFixture(bool pictures) {
    if (pictures) root.setCellPixelSize(16, 8);
    table.setColumns({{.title = "", .width = 2, .sortable = false, .icon = true}, {.title = "Name", .width = 10}, {.title = "Price", .width = 8}});
    table.resize(6, 30);
    table.move(1, 0);
    table.setModel(std::make_shared<IconModel>());
    root.refresh();
  }
};

}  // namespace

TEST(table_shows_an_icon_column_as_one_picture_for_every_row_that_has_it) {
  IconFixture f(true);
  f.root.refresh();
  if (f.root.images().empty()) return;  // no pixel backend in this build
  CHECK(f.root.images().size() == 2);
  CHECK(f.root.images()[0].shared.valid() && f.root.images()[0].shared == f.root.images()[1].shared);
}

TEST(table_icon_column_takes_no_room_where_the_display_has_no_pictures) {
  IconFixture plain(false);
  CHECK(plain.root.images().empty());
  const std::string text = plain.buffer.toPlainText();
  const size_t heading = text.find("Name"), nokia = text.find("4.20");
  CHECK(heading != std::string::npos && nokia != std::string::npos);
  // No gap before the first visible column: its heading and its first cell start together.
  CHECK(heading - text.rfind('\n', heading) == nokia - text.rfind('\n', nokia));
}

namespace {

// Name at 0, Price at 9, and the buttons at 16 ("[i]") and 20 ("[c]").
struct ButtonFixture : Fixture {
  struct Click {
    int row, column, button;
  };
  std::vector<Click> clicks;
  Connection connection;
  ButtonFixture() {
    table.setColumns({{.title = "Name", .width = 8}, {.title = "Price", .width = 6, .align = Align::RIGHT}, {.title = "", .width = 0, .sortable = true, .buttons = {"[i]", "[c]"}}});
    connection = table.button_clicked.connect([this](int r, int c, int b) { clicks.push_back({r, c, b}); });
  }
};

}  // namespace

TEST(table_buttons_column_draws_its_glyphs_and_a_click_on_one_says_which_row_and_button_and_leaves_the_selection) {
  ButtonFixture f;
  CHECK(f.text().find("[i] [c]") != std::string::npos);
  int selected = -2;
  auto a = f.table.selection_changed.connect([&](int r) { selected = r; });
  f.table.select(3);
  selected = -2;
  f.click(3, 17);  // the second row's "[i]"
  CHECK(selected == -2 && f.table.selectedRow() == 3);  // the selection stayed
  CHECK(f.clicks.size() == 1u); if (f.clicks.empty()) return;
  CHECK(f.clicks[0].row == 1 && f.clicks[0].column == 2 && f.clicks[0].button == 0);
  f.click(4, 21);  // the third row's "[c]"
  CHECK(f.clicks.size() == 2u); if (f.clicks.empty()) return;
  CHECK(f.clicks[1].row == 2 && f.clicks[1].button == 1);
}

TEST(table_click_beside_a_button_or_off_it_only_selects) {
  ButtonFixture f;
  f.click(2, 19);  // the gap between the buttons
  CHECK(f.table.selectedRow() == 0 && f.clicks.empty());
  f.press(3, 17);
  f.release(3, 5);  // pressed on a button, released elsewhere in the row
  CHECK(f.clicks.empty());
  f.press(3, 17);
  f.release(4, 17);  // released on the same button of another row
  CHECK(f.clicks.empty());
}

TEST(table_buttons_follow_the_rows_through_a_sort_and_keep_the_scroll_position) {
  ButtonFixture f;
  f.click(1, 17);  // the heading over the buttons does not sort
  CHECK(f.table.sortOrder() == SortOrder::NONE);
  f.click(1, 2);  // Name ascending: Fortum Kone Nokia Sampo
  f.click(2, 17);  // the first row shown is Fortum, model row 1
  CHECK(f.clicks.size() == 1u); if (f.clicks.empty()) return;
  CHECK(f.clicks[0].row == 1);
  f.table.resize(3, 30);  // heading and two rows
  f.table.post(Wheel{0, 0, 2, 3, 0, -1, {}});
  CHECK(f.table.firstVisiblePosition() == 1);
  f.click(2, 17);  // the first row in view is position 1: Kone
  CHECK(f.clicks.back().row == 3);
  CHECK(f.table.firstVisiblePosition() == 1);  // the click did not move the view
  CHECK(f.table.sortColumn() == 0);
}

TEST(table_reselect_keeps_the_view_where_it_is) {
  Fixture f;
  f.table.resize(3, 30);  // heading and two rows
  f.table.reselect(3);    // the selection comes back after new data: no scrolling to it
  CHECK(f.table.selectedRow() == 3);
  CHECK(f.table.firstVisiblePosition() == 0);
  f.table.select(2);  // a user's own selection brings the row into view
  f.table.select(0);
  f.table.reselect(3);
  CHECK(f.table.firstVisiblePosition() == 0);
}
