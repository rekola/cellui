#pragma once

#include <cellui/Charts.h>
#include <cellui/ModalDialog.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cellui {

// What a ChartModal shows: one series and what it is called.
struct ChartData {
  std::string title;
  // The values, oldest first. A value that is not a number is left out (with its label).
  std::vector<float> values;
  // What each value is at (a date, a time), one for each value, or none. They are the chart's x axis: the first and the
  // last under its ends, and the one the cursor is at beside the title.
  std::vector<std::string> labels;
  // How bright the line is at each value, 0 to 1, one for each value (TraceLook::light: the volume traded at each point, say), or none
  // for an even line. A value left out takes its brightness with it.
  std::vector<float> light;
  // A value as text, for the axis and the reading under the cursor; a short number by default. The chart knows no unit.
  std::function<std::string(float)> format;
  // The line's colour (the theme's by default).
  std::optional<Color> color;
};

// A series drawn large over the dimmed screen, and nothing else: no window around it, only the line with its axis labels, the
// title and a cursor with a reading (where the display draws pictures the cursor is a hairline and a dot on the line; in cells only the reading shows it). It is drawn by the display's Charts::drawTrace(), the same line a table's trace
// column draws, so it is pictures where the display has them and braille where it has not. The cursor starts at the latest
// value; Left and Right (Ctrl+B, Ctrl+F) move it, Page Up and Page Down by a tenth, Home and End to the ends, and the pointer
// moves it over the chart. Escape, Enter, q or Ctrl+G close it, and so does a click outside the chart.
// showModal() returns 0.
class ChartModal : public ModalDialog {
 public:
  ChartModal(Widget & root, Plane & screen, EventLoop & loop, const Charts & charts, ChartData data);

  // The value the cursor is on, as the series is now (after values that are not numbers are left out).
  size_t cursor() const { return cursor_; }
  size_t size() const { return values_.size(); }
  void setCursor(size_t index);
  // The reading under the cursor as the chart shows it: "2026-11-20  4.52".
  std::string reading() const;

 protected:
  void layout(Plane & screen, int rows, int cols) override;
  void keyDown(const KeyDown & e) override;
  void mouseDown(const MouseDown & e) override;
  void mouseMove(const MouseMove & e) override;
  void close() override { plane_.reset(); }

 private:
  std::string formatted(float v) const;
  // The value under a screen column of the chart, or -1 where the chart is not.
  int indexAt(int screen_row, int screen_col) const;
  bool inside(int screen_row, int screen_col) const;

  const Charts & charts_;
  std::string title_;
  std::vector<float> values_;
  std::vector<std::string> labels_;
  std::vector<float> light_;  // as bright as the values are long, or empty; the chart draws from it while it is up
  std::function<std::string(float)> format_;
  std::optional<Color> color_;
  size_t cursor_ = 0;
  std::unique_ptr<Plane> plane_;
  // Where it is on the screen, and the chart's box in it, as the last layout left them.
  int top_ = 0, left_ = 0, rows_ = 0, cols_ = 0;
  int chart_row_ = 0, chart_col_ = 0, chart_rows_ = 0, chart_cols_ = 0;
};

}  // namespace cellui
