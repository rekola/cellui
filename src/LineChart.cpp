#include <cellui/LineChart.h>

#include <cellui/Plane.h>
#include <cellui/StyleProvider.h>

#include <algorithm>
#include <cmath>

namespace cellui {

bool LineChart::setValues(const std::vector<float> & values, std::pair<float, float> range) {
  auto [rows, cols] = getDim();
  if (rows <= 0 || cols <= 0 || values.empty()) return false;

  // If no value has moved by a step the chart can draw (a pixel, or a dot of a glyph cell) from where it was drawn,
  // what is drawn would be the same.
  int cell_h = 0, cell_w = 0;
  const float steps = static_cast<float>(rows * (getPlane().cellPixelSize(cell_h, cell_w) && cell_h > 0 ? cell_h : 4));
  const float span = range.second - range.first;
  bool same = values.size() == last_values_.size() && rows == last_rows_ && cols == last_cols_ && range == last_range_;
  for (size_t i = 0; same && i < values.size(); i++) same = std::fabs(values[i] - last_values_[i]) * steps < std::fabs(span);
  if (same) return false;
  last_values_ = values;
  last_range_ = range;
  draw();
  return true;
}

void LineChart::draw() {
  auto [rows, cols] = getDim();
  last_rows_ = rows;
  last_cols_ = cols;
  const StyleProvider & styles = getPlane().getStyles();
  setBgColor(styles.window_bg_color);
  fill();
  if (rows <= 0 || cols <= 0 || last_values_.empty()) return;
  TraceLook look{styles.window_bg_color, styles.line_high_color, styles.line_high_color};
  look.range = last_range_;
  look.light_from = LightFrom::HEIGHT;  // one series and nothing else to say how bright: the higher, the brighter
  // Opaque, so a picture never composites against the raw terminal background.
  charts_.drawTrace(getPlane(), 0, 0, rows, cols, last_values_, look, 0, rows, 0);
}

void LineChart::onResize() {
  auto [rows, cols] = getDim();
  if (!last_values_.empty() && (rows != last_rows_ || cols != last_cols_)) draw();
}

void LineChart::clear() {
  last_values_.clear();
  draw();
}

}  // namespace cellui
