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
  void click(int row, int col) { table.post(MouseDown{static_cast<float>(col), static_cast<float>(row), row, col, 1, {}}); }
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
