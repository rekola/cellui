#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/ChartModal.h>
#include <cellui/InputDispatcher.h>
#include <cellui/PlatformEventLoop.h>

#include <set>

using namespace cellui;

namespace {

Input press(int id, unsigned mods = 0) { return keyInput(id, mods, Input::Kind::PRESS); }
Input mouse(int id, int row, int col, Input::Kind kind) { return Input(id, row, col, false, false, false, false, kind); }

Input mouseAt(int id, int row, int col, float fy, float fx, Input::Kind kind) {
  return Input(id, row, col, false, false, false, false, kind).withPosition(fy, fx);
}

// A widget tree under a screen and a loop that turns without waiting; each turn runs one step and the loop ends when they run out.
struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{30, 90};
  BufferPlane screen{buffer, styles};
  std::unique_ptr<Charts> charts = cellCharts();
  Widget root;
  InputDispatcher dispatcher{root};
  PlatformEventLoop loop;
  size_t turn = 0;
  int widget_keys = 0;
  Connection widget_connection;
  std::vector<std::function<void()>> steps;
  Connection frame;

  Fixture() {
    loop.setBefore([] { return 0; });
    widget_connection = root.on<KeyDown>([this](const KeyDown &, EventContext &) {
      widget_keys++;
      return Handled::no;
    });
    frame = loop.onFrame([this] {
      if (turn < steps.size()) steps[turn++]();
      else loop.quit();
    });
  }
  void key(int id, unsigned mods = 0) {
    steps.push_back([this, id, mods] { dispatcher.offer(press(id, mods)); });
  }
  void click(int row, int col) {
    steps.push_back([this, row, col] {
      dispatcher.offer(mouse(Key::Button1, row, col, Input::Kind::PRESS));
      dispatcher.offer(mouse(Key::Button1, row, col, Input::Kind::RELEASE));
    });
  }
  void hover(int row, int col) {
    steps.push_back([this, row, col] { dispatcher.offer(mouse(Key::Motion, row, col, Input::Kind::UNKNOWN)); });
  }
  void then(std::function<void()> check) { steps.push_back(std::move(check)); }
  std::string screenText() {
    screen.refresh();
    return buffer.toPlainText();
  }
};

ChartData prices() {
  ChartData d;
  d.title = "Nokia 3 months";
  d.values = {4.0f, 4.5f, 5.0f, 4.25f, 3.0f, 3.5f};
  d.labels = {"2026-08-01", "2026-08-02", "2026-08-03", "2026-08-04", "2026-08-05", "2026-08-06"};
  d.format = [](float v) { return std::to_string(static_cast<int>(v * 100)); };
  return d;
}

}  // namespace

TEST(chart_modal_reading_ends_with_the_note_of_the_value_and_the_axis_has_none) {
  Fixture f;
  ChartData d = prices();
  d.notes = {"vol 1", "", "vol 3", "vol 4", "vol 5", "vol 6"};
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, std::move(d));
  CHECK(chart.reading() == "2026-08-06  350  vol 6");
  chart.setCursor(1);
  CHECK(chart.reading() == "2026-08-02  450");
}

TEST(chart_modal_shows_the_title_the_axis_and_the_latest_value_and_nothing_else) {
  Fixture f;
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, prices());
  f.then([&] {
    const std::string t = f.screenText();
    CHECK(t.find("Nokia 3 months") != std::string::npos);
    CHECK(t.find("2026-08-06  350") != std::string::npos);  // the reading: where the cursor is, the latest
    CHECK(t.find("500") != std::string::npos && t.find("300") != std::string::npos);  // the highest and the lowest on the value axis
    CHECK(t.find("2026-08-01") != std::string::npos);  // the first label under the chart's left end
    CHECK(t.find("OK") == std::string::npos && t.find("Cancel") == std::string::npos);
    CHECK(chart.cursor() == 5);
  });
  f.key(Key::Enter);
  CHECK(chart.showModal() == 0);
  CHECK(f.root.activeModal() == nullptr);
}

TEST(chart_modal_arrow_keys_move_the_cursor_and_the_reading_follows) {
  Fixture f;
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, prices());
  f.key(Key::Left);
  f.then([&] { CHECK(chart.cursor() == 4 && chart.reading() == "2026-08-05  300"); });
  f.key(Key::Home);
  f.then([&] { CHECK(chart.cursor() == 0 && chart.reading() == "2026-08-01  400"); });
  f.key(Key::Left);  // not past the first
  f.then([&] { CHECK(chart.cursor() == 0); });
  f.key('f', kModCtrl);
  f.then([&] { CHECK(chart.cursor() == 1); });
  f.key(Key::End);
  f.then([&] { CHECK(chart.cursor() == 5 && f.screenText().find("2026-08-06  350") != std::string::npos); });
  f.key('q');
  CHECK(chart.showModal() == 0);
  CHECK(f.widget_keys == 0);  // the chart took every key
}

TEST(chart_modal_draws_no_cursor_in_cells_but_the_reading_follows_it) {
  Fixture f;
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, prices());
  // The chart's own cells are the same wherever the cursor is: only the reading above it changes.
  auto chartRows = [&] {
    std::string text = f.screenText(), out;
    int row = 0;
    for (size_t i = 0; i < text.size(); i++) {
      if (text[i] == '\n') row++;
      else if (row >= 7 && row <= 22) out += text[i];  // under the title row, above the axis labels
    }
    return out;
  };
  std::string at_end, at_start;
  f.then([&] { at_end = chartRows(); });
  f.key(Key::Home);
  f.then([&] { at_start = chartRows(); });
  f.key(Key::Escape);
  chart.showModal();
  CHECK(!at_end.empty() && at_end == at_start);
}

TEST(chart_modal_the_pointer_over_the_chart_moves_the_cursor_and_a_click_away_closes_it) {
  Fixture f;
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, prices());
  // The chart is in the middle of the 30 x 90 screen, its left end after the margin and the value labels (columns 18 to 75).
  f.hover(15, 19);
  f.then([&] { CHECK(chart.cursor() == 0); });
  f.click(15, 47);
  f.then([&] { CHECK(chart.cursor() >= 2 && chart.cursor() <= 3 && f.root.activeModal() != nullptr); });
  f.click(0, 0);  // away from it
  CHECK(chart.showModal() == 0);
  CHECK(f.root.activeModal() == nullptr);
}

TEST(chart_modal_leaves_out_values_that_are_not_numbers_with_their_labels) {
  Fixture f;
  ChartData d = prices();
  d.values[2] = std::numeric_limits<float>::quiet_NaN();
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, d);
  CHECK(chart.size() == 5);
  f.key(Key::Home);
  f.key(Key::Right);
  f.key(Key::Right);
  f.then([&] { CHECK(chart.reading() == "2026-08-04  425"); });  // the third value is the fourth day
  f.key(Key::Escape);
  chart.showModal();
}

TEST(chart_modal_a_series_with_nothing_in_it_is_a_title_and_still_closes) {
  Fixture f;
  ChartData d;
  d.title = "Nothing";
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, d);
  f.then([&] { CHECK(f.screenText().find("Nothing") != std::string::npos && chart.reading().empty()); });
  f.key(Key::Right);
  f.key(Key::Escape);
  CHECK(chart.showModal() == 0);
}

TEST(chart_modal_a_precise_pointer_reaches_every_point_not_one_per_cell) {
  Fixture f;
  ChartData d = prices();
  d.values.clear();
  for (int i = 0; i < 300; i++) d.values.push_back(static_cast<float>(i % 17));
  d.labels.clear();
  ChartModal chart(f.root, f.screen, f.loop, *f.charts, std::move(d));
  // Several positions inside one cell (column 40) give several points.
  std::set<size_t> seen;
  for (int k = 0; k < 8; k++) {
    const float x = 40.0f + 0.125f * static_cast<float>(k) + 0.0625f;
    f.steps.push_back([&f, x] { f.dispatcher.offer(mouseAt(Key::Motion, 15, 40, 15.5f, x, Input::Kind::UNKNOWN)); });
    f.then([&] { seen.insert(chart.cursor()); });
  }
  f.then([&] { CHECK(seen.size() >= 4); });
  chart.showModal();
}
