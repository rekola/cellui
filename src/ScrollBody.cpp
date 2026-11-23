#include <cellui/ScrollBody.h>

#include <algorithm>
#include <cmath>

namespace cellui {

void ScrollBody::layout(Plane & screen, Plane & box, int box_row, int box_col, int top, int rows, int cols, const ScrollBodyColors & colors) {
  rows_ = std::max(1, rows);
  cols_ = cols;
  row_ = box_row + top;
  col_ = box_col;
  list_.setSmooth(screen.smoothScroll());
  // With a cursor the window follows the selection; without one it only goes where it is scrolled.
  if (cursor_) list_.firstVisible(rows_);
  else list_.scroll(0, rows_);
  const auto now = std::chrono::steady_clock::now();
  if (fresh_) {
    list_.snap();
    cursor_y_ = target();
    fresh_ = false;
    last_tick_ = now;
  }
  if (clocked_) {
    tick(std::min(0.1, std::chrono::duration<double>(now - last_tick_).count()));
    last_tick_ = now;
  }
  if (!list_.smooth()) cursor_y_ = target();
  cursor_y_ = std::clamp(cursor_y_, 0.0, static_cast<double>(rows_ - 1));

  // The scrollbar rides the box's last column, beside the lines.
  box.setBgColor(colors.background);
  for (int i = 0; i < rows_; i++) box.putstr(top + i, cols, " ");
  drawScrollbar(box, top, cols, rows_, list_.count(), rows_, list_.shown(), colors.background, colors.track, colors.thumb);

  if (!plane_) {
    plane_ = screen.createChild();
    plane_->setElevation(box.elevation());
    plane_->setCastsShadow(false);
  }
  plane_->setMouseCursor(mouse_cursor_);
  plane_->move(row_, col_);
  plane_->resize(rows_, cols);
  plane_->setScrollOffset(list_.fraction(), 0);
  plane_->erase();
  plane_->setFgColor(colors.foreground);
  plane_->setBgColor(colors.background);
  // The cursor is not drawn while a scroll of the wheel has left the selection out of view.
  const double at = target();
  if (cursor_ && list_.count() > 0 && at >= 0 && at <= rows_ - 1) plane_->setBand(Band{cursor_y_, 1.0, 0.0, static_cast<double>(cols), colors.cursor, 0.45f});
  else plane_->clearBand();
  plane_->moveToTop();
}

void ScrollBody::tick(double seconds) {
  list_.tick(seconds);
  if (!cursor_ || !list_.smooth()) return;
  const double at = target();
  cursor_y_ += (at - cursor_y_) * (1.0 - std::exp(-std::max(0.0, seconds) / ScrollAxis::kSettleSeconds));
  if (std::abs(at - cursor_y_) < ScrollAxis::kSnap) cursor_y_ = at;
}

bool ScrollBody::animating() const {
  if (!plane_) return false;
  return list_.animating() || (cursor_ && list_.smooth() && cursor_y_ != target());
}

int ScrollBody::lineAt(int row, int col) const {
  const int r = row - row_, c = col - col_;
  if (r < 0 || r >= rows_ || c < 0 || c > cols_) return -1;
  const int index = list_.indexAt(r);
  return index >= 0 && index < list_.count() ? index : -1;
}

bool ScrollBody::onBar(int row, int col) const {
  const int r = row - row_;
  return col - col_ == cols_ && r >= 0 && r < rows_ && list_.count() > rows_;
}

bool ScrollBody::press(int row, int col, bool inside_box) {
  pressing_ = true;
  if (onBar(row, col) && list_.beginThumbDrag(rows_, row - row_)) {
    started_on_line_ = started_outside_ = false;
    return true;
  }
  const int index = lineAt(row, col);
  started_on_line_ = index >= 0;
  started_outside_ = !inside_box;
  list_.setPressed(index);
  return false;
}

bool ScrollBody::drag(int row, int col) {
  if (list_.thumbDragging()) {
    list_.dragThumb(rows_, row - row_);
    return true;
  }
  if (pressing_ && started_on_line_) list_.setPressed(lineAt(row, col));
  return false;
}

bool ScrollBody::release(int row, int col) {
  if (!list_.thumbDragging()) return false;
  list_.dragThumb(rows_, row - row_);
  list_.endThumbDrag();
  pressing_ = false;
  return true;
}

ScrollBody::Click ScrollBody::click(int row, int col, bool inside_box) {
  if (release(row, col)) return Click::NONE;
  const int index = lineAt(row, col);
  const bool on_line = pressing_ ? started_on_line_ : index >= 0;
  const bool outside = pressing_ ? started_outside_ : !inside_box;
  pressing_ = false;
  list_.setPressed(-1);
  if (on_line && index >= 0) {
    list_.select(index);
    return Click::CHOSEN;
  }
  return outside && !inside_box ? Click::OUTSIDE : Click::NONE;
}

}  // namespace cellui
