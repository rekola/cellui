#pragma once

#include <cellui/Charts.h>
#include <cellui/Widget.h>

#include <utility>
#include <vector>

namespace cellui {

// A curve over the plane's width, filled below it: a series of values spaced evenly from left to right, drawn by the
// display's Charts::drawTrace() - the same line a table's trace column and a ChartModal draw, so there is one line
// chart. What the values mean (a spectrum, a history, a profile) is the application's.
class LineChart : public Widget {
public:
  LineChart(Plane & parent, const Charts & charts) : Widget(parent), charts_(charts) { }
  ~LineChart() override = default;

  // A fresh curve through `values`, the bottom of the plane standing for `range.first` and the top for `range.second`
  // (a value outside is drawn at the edge). Redraws, unless the curve is where it was drawn; returns whether it did.
  bool setValues(const std::vector<float> & values, std::pair<float, float> range = {0.0f, 1.0f});

  // Shows no curve at all. Redraws.
  void clear();

 protected:
  void onResize() override;

 private:
  void draw();

  const Charts & charts_;
  // What was last drawn: the values, their range and the size.
  std::vector<float> last_values_;
  std::pair<float, float> last_range_{0.0f, 1.0f};
  int last_rows_ = 0, last_cols_ = 0;
};

}  // namespace cellui
