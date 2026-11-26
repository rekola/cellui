#include <cellui/ChartModal.h>

#include <cellui/Input.h>
#include <cellui/Utf8.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cellui {

namespace {

// The chart takes this much of the screen, at most so many cells, and keeps a margin of cells round what it draws.
constexpr int kMaxCols = 110, kMaxRows = 30;
constexpr int kMarginCols = 3, kMarginRows = 1;
constexpr float kLineWidth = 2.0f;
constexpr double kBorderWidth = 1.0;  // logical pixels
constexpr float kBorderAlpha = 0.35f;  // faint: the line is what the eye is meant to go to
constexpr int kMinChartRows = 3;

// A short number: five significant digits, a point whatever the locale.
std::string shortNumber(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.5g", static_cast<double>(v));
  std::string s = buf;
  std::replace(s.begin(), s.end(), ',', '.');
  return s;
}

const TextStyle kLeft{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::LEFT};
const TextStyle kRight{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::RIGHT};
const TextStyle kTitle{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kBoldFace, TextAlign::LEFT};

}  // namespace

ChartModal::ChartModal(Widget & root, Plane & screen, EventLoop & loop, const Charts & charts, ChartData data)
    : ModalDialog(root, screen, loop), charts_(charts), title_(std::move(data.title)), format_(std::move(data.format)), color_(data.color) {
  const bool labelled = data.labels.size() == data.values.size(), lit = data.light.size() == data.values.size(), noted = data.notes.size() == data.values.size();
  for (size_t i = 0; i < data.values.size(); i++) {
    if (!std::isfinite(data.values[i])) continue;
    values_.push_back(data.values[i]);
    if (labelled) labels_.push_back(std::move(data.labels[i]));
    if (noted) notes_.push_back(std::move(data.notes[i]));
    if (lit) light_.push_back(data.light[i]);
  }
  if (!values_.empty()) cursor_ = values_.size() - 1;
}

std::string ChartModal::formatted(float v) const { return format_ ? format_(v) : shortNumber(v); }

void ChartModal::setCursor(size_t index) {
  if (values_.empty()) return;
  index = std::min(index, values_.size() - 1);
  if (index == cursor_) return;
  cursor_ = index;
  relayout();
}

std::string ChartModal::reading() const {
  if (values_.empty()) return {};
  std::string text = formatted(values_[cursor_]);
  if (!labels_.empty() && !labels_[cursor_].empty()) text = labels_[cursor_] + "  " + text;
  if (!notes_.empty() && !notes_[cursor_].empty()) text += "  " + notes_[cursor_];
  return text;
}

bool ChartModal::inside(int screen_row, int screen_col) const {
  return screen_row >= top_ && screen_row < top_ + rows_ && screen_col >= left_ && screen_col < left_ + cols_;
}

int ChartModal::indexAt(int screen_row, int screen_col, float screen_x) const {
  const int row = screen_row - top_, col = screen_col - left_;
  if (values_.empty() || row < chart_row_ || row >= chart_row_ + chart_rows_ || col < chart_col_ || col >= chart_col_ + chart_cols_) return -1;
  // The pointer's own position, not its cell's: with a fraction (where the display has a precise pointer) the points are told
  // apart by less than a cell. A whole x is the pointer somewhere in its cell: take the middle.
  double x = static_cast<double>(screen_x) - left_;
  if (x == std::floor(x)) x += 0.5;
  const double f = std::clamp((x - chart_col_) / static_cast<double>(chart_cols_), 0.0, 1.0);
  return static_cast<int>(std::clamp<long>(std::lround(f * static_cast<double>(values_.size() - 1)), 0, static_cast<long>(values_.size()) - 1));
}

void ChartModal::layout(Plane & screen, int rows, int cols) {
  rows_ = std::min(std::min(std::max(rows * 7 / 10, 10), kMaxRows), rows);
  cols_ = std::min(std::min(std::max(cols * 3 / 4, 30), kMaxCols), cols);
  if (!plane_) {
    plane_ = screen.createChild();
    plane_->setMouseCursor(MouseCursor::DEFAULT);
    // Over the scrim, with no raised area of its own (no shadow): only the chart, in a box with rounded corners and a thin border in
    // the line's colour where the display draws them (the window; a terminal shows neither).
    plane_->setElevation(screen.getStyles().theme.elevation.modal);
    plane_->setElevationArea(false);
    plane_->setCornerRadius(screen.getStyles().theme.dialog_radius_px);
    plane_->setBorder(color_.value_or(screen.getStyles().line_high_color), kBorderWidth, kBorderAlpha);
  }
  plane_->resize(rows_, cols_);
  const StyleProvider & styles = plane_->getStyles();
  const Color bg = styles.window_bg_color, fg = styles.window_fg_color, strong = styles.window_accent_fg_color, quiet = styles.theme.text_secondary;
  plane_->setFgColor(fg);
  plane_->setBgColor(bg);
  for (int y = 0; y < rows_; y++) plane_->putstr(y, 0, std::string(static_cast<size_t>(cols_), ' '));

  // The title and the reading on the top row, the chart under them with its value labels at the left, the first and the last
  // label of the x axis on the bottom row.
  const int inner = cols_ - 2 * kMarginCols;
  const std::string text = reading();
  const int reading_cols = std::min(inner / 2, utf8::displayWidth(text) + 2);
  plane_->setFgColor(strong);
  plane_->putText(kMarginRows, kMarginCols, std::max(1, inner - reading_cols - 1), title_, kTitle);
  plane_->setFgColor(fg);
  plane_->putText(kMarginRows, cols_ - kMarginCols - reading_cols, reading_cols, text, kRight);

  chart_row_ = kMarginRows + 1;
  chart_rows_ = std::max(kMinChartRows, rows_ - 2 * kMarginRows - 2);
  const int axis_row = chart_row_ + chart_rows_;  // the x labels, under the chart
  if (values_.empty()) {
    chart_cols_ = 0;
  } else {
    float low = values_[0], high = values_[0];
    for (float v : values_) {
      low = std::min(low, v);
      high = std::max(high, v);
    }
    std::string top = formatted(high), bottom = formatted(low), middle = formatted(low + (high - low) / 2);
    const bool flat = !(high > low);
    int gutter = std::max(utf8::displayWidth(top), utf8::displayWidth(bottom));
    if (flat) gutter = utf8::displayWidth(top);
    gutter = std::min(gutter + 1, inner / 3);
    chart_col_ = kMarginCols + gutter + 1;
    chart_cols_ = std::max(1, cols_ - kMarginCols - chart_col_);

    plane_->setFgColor(quiet);
    if (flat) {
      plane_->putText(chart_row_ + chart_rows_ / 2, kMarginCols, gutter, top, kRight);
    } else {
      plane_->putText(chart_row_, kMarginCols, gutter, top, kRight);
      if (chart_rows_ >= 7) plane_->putText(chart_row_ + chart_rows_ / 2, kMarginCols, gutter, middle, kRight);
      plane_->putText(chart_row_ + chart_rows_ - 1, kMarginCols, gutter, bottom, kRight);
    }
    if (!labels_.empty()) {
      const int half = chart_cols_ / 2;
      plane_->putText(axis_row, chart_col_, std::max(1, half), labels_.front(), kLeft);
      if (values_.size() > 1) plane_->putText(axis_row, chart_col_ + chart_cols_ - std::max(1, half), std::max(1, half), labels_.back(), kRight);
    }

    TraceLook look{bg, color_.value_or(styles.line_high_color), color_.value_or(styles.line_high_color)};
    look.light = light_;
    look.line_width = kLineWidth;
    if (pointer_away_) look.cursor.reset();
    else look.cursor = values_.size() > 1 ? static_cast<float>(cursor_) / static_cast<float>(values_.size() - 1) : 0.5f;
    charts_.drawTrace(*plane_, chart_row_, chart_col_, chart_rows_, chart_cols_, values_, look, 0, rows_, 1);
  }

  top_ = (rows - rows_) / 2;
  left_ = (cols - cols_) / 2;
  plane_->move(top_, left_);
  plane_->moveToTop();
}

void ChartModal::keyDown(const KeyDown & e) {
  showCursor();
  const bool plain = !e.mods.alt && !e.mods.meta;
  if (e.key == Key::Escape || e.key == Key::Enter || (plain && !e.mods.ctrl && e.key == 'q') || (e.mods.ctrl && e.key == 'g')) {
    done(0);
    return;
  }
  const long last = static_cast<long>(values_.size()) - 1;
  const long page = std::max<long>(1, static_cast<long>(values_.size()) / 10);
  long to = static_cast<long>(cursor_);
  if (e.key == Key::Left || (e.mods.ctrl && e.key == 'b')) to -= 1;
  else if (e.key == Key::Right || (e.mods.ctrl && e.key == 'f')) to += 1;
  else if (e.key == Key::PageUp) to -= page;
  else if (e.key == Key::PageDown) to += page;
  else if (e.key == Key::Home) to = 0;
  else if (e.key == Key::End) to = last;
  else return;
  setCursor(static_cast<size_t>(std::clamp<long>(to, 0, std::max<long>(0, last))));
}

void ChartModal::mouseDown(const MouseDown & e) {
  if (e.button != 1) return;
  if (inside(e.row, e.col)) showCursor();
  if (const int i = indexAt(e.row, e.col, e.x); i >= 0) setCursor(static_cast<size_t>(i));
  else if (!inside(e.row, e.col)) done(0);  // a click away from the chart puts it away
}

void ChartModal::mouseMove(const MouseMove & e) {
  // The cursor is drawn while the pointer is over the box; away from it only the reading stays.
  if (inside(e.row, e.col)) showCursor();
  else if (!pointer_away_) {
    pointer_away_ = true;
    relayout();
  }
  if (const int i = indexAt(e.row, e.col, e.x); i >= 0) setCursor(static_cast<size_t>(i));
}

void ChartModal::showCursor() {
  if (!pointer_away_) return;
  pointer_away_ = false;
  relayout();
}

}  // namespace cellui
