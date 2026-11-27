#include <cellui/ScrollBody.h>

#include <algorithm>

namespace cellui {

void ScrollBody::layout(Plane & screen, Plane & box, int box_row, int box_col, int top, int rows, int cols, const Color & background,
                        const Color & foreground, const Color & track, const Color & thumb) {
  rows = std::max(1, rows);
  axis_.setSmooth(screen.smoothScroll());
  axis_.setExtent(count_, rows);
  const auto now = std::chrono::steady_clock::now();
  if (fresh_) {
    axis_.setSmooth(false);
    axis_.setSmooth(screen.smoothScroll());
    last_tick_ = now;
    fresh_ = false;
  }
  axis_.tick(std::min(0.1, std::chrono::duration<double>(now - last_tick_).count()));
  last_tick_ = now;

  // The scrollbar rides the box's last column, beside the lines.
  box.setInkOnGlass(foreground, background);
  for (int i = 0; i < rows; i++) box.putstr(top + i, cols, " ");
  drawScrollbar(box, top, cols, rows, count_, rows, axis_.shown(), background, track, thumb, box.frostedPopups());
  bar_row_ = box_row + top;
  bar_col_ = box_col + cols;
  bar_rows_ = rows;

  if (!plane_) {
    plane_ = screen.createChild();
    plane_->setMouseCursor(MouseCursor::DEFAULT);
    plane_->setElevation(box.elevation());
    plane_->setCastsShadow(false);
    // It lies over the box's own border column; the border stays.
    plane_->setCoversBorders(false);
  }
  plane_->move(box_row + top, box_col);
  plane_->resize(rows, cols);
  plane_->setScrollOffset(axis_.fraction(), 0);
  plane_->erase();
  plane_->setInkOverGlass(foreground, background);
  for (int i = 0; i < drawnRows(); i++) plane_->putstr(i, 0, std::string(static_cast<size_t>(cols), ' '));
  plane_->moveToTop();
}

bool ScrollBody::onBar(int row, int col) const {
  return col == bar_col_ && row >= bar_row_ && row < bar_row_ + bar_rows_ && count_ > bar_rows_;
}

bool ScrollBody::press(int row, int col) {
  if (!onBar(row, col)) return false;
  auto jump = drag_.begin(bar_rows_, count_, bar_rows_, axis_.shown(), track(row));
  if (jump) axis_.jumpTo(*jump);
  return drag_.active();
}

bool ScrollBody::drag(int row, int) {
  if (!drag_.active()) return false;
  axis_.jumpTo(drag_.offset(bar_rows_, count_, bar_rows_, track(row)));
  return true;
}

bool ScrollBody::release(int row, int col) {
  if (!drag_.active()) return false;
  drag(row, col);
  drag_.end();
  return true;
}

}  // namespace cellui
