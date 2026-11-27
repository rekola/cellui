#pragma once

#include <cellui/Color.h>
#include <cellui/Plane.h>
#include <cellui/Scrollbar.h>
#include <cellui/Viewport.h>

#include <chrono>
#include <memory>

namespace cellui {

// The scrolling body of a dialog: a column of one-row lines on a plane of its own over the dialog's box, with a
// scrollbar in the box's last column. It is everything a list needs to scroll as the choice dialog and the palette
// do, so a dialog that draws lines and does not want a cursor gets it by owning one: the display's smooth scrolling
// (the lines ease by parts of a row), a draggable scrollbar, and the wheel that moves the view. The dialog draws the
// lines it is told to (drawnRows() of them, from firstLine()) in the display's proportional face, and passes it the
// keys and pointer events as plain values.
class ScrollBody {
 public:
  // The number of lines there are.
  void setCount(int count) { count_ = count < 0 ? 0 : count; }
  int count() const { return count_; }

  // Places the body over `box` (a plane of the dialog, at `box_row`, `box_col` of the screen), `rows` lines high from
  // row `top` of the box, `cols` wide; the scrollbar is in column `cols` of the box. Eases the view by the time since
  // the last call, erases the body and leaves it with `foreground` set, on `background` or, over a dialog box that is glass, on no background of its own (dialog::ink()), for the dialog to draw into
  // with plane(): line i of drawnRows() is on row i, and is line firstLine() + i. A new body starts at the top.
  void layout(Plane & screen, Plane & box, int box_row, int box_col, int top, int rows, int cols, const Color & background,
              const Color & foreground, const Color & track, const Color & thumb);
  Plane & plane() { return *plane_; }
  int firstLine() const { return axis_.firstShown(); }
  // One more than shown where the display scrolls by parts of a row.
  int drawnRows() const { return axis_.view() + (axis_.smooth() ? 1 : 0); }

  // Scrolling, by lines; the wheel is the dialog's to scale.
  void scrollBy(int delta) { axis_.scrollBy(delta); }
  void home() { axis_.scrollTo(0); }
  void end() { axis_.scrollTo(axis_.maxOffset()); }
  int page() const { return axis_.view() > 1 ? axis_.view() - 1 : 1; }

  // The left button at a screen cell: whether it is the scrollbar's, which the body then follows until release().
  bool press(int row, int col);
  bool drag(int row, int col);
  bool release(int row, int col);
  bool dragging() const { return drag_.active(); }

  // Whether it is still easing, so another layout is due.
  bool animating() const { return plane_ && axis_.animating(); }
  void close() { plane_.reset(); }

 private:
  bool onBar(int row, int col) const;
  int track(int row) const { return row - bar_row_; }

  int count_ = 0;
  ScrollAxis axis_;
  ScrollbarDrag drag_;
  std::unique_ptr<Plane> plane_;
  std::chrono::steady_clock::time_point last_tick_;
  bool fresh_ = true;
  // Where the scrollbar is on the screen.
  int bar_row_ = 0, bar_col_ = 0, bar_rows_ = 0;
};

}  // namespace cellui
