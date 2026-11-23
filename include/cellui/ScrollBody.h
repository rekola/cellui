#pragma once

#include <cellui/ChoiceList.h>
#include <cellui/Color.h>
#include <cellui/MouseCursor.h>
#include <cellui/Plane.h>
#include <cellui/Scrollbar.h>

#include <chrono>
#include <memory>

namespace cellui {

struct ScrollBodyColors {
  Color background, foreground;
  // The scrollbar's line and thumb.
  Color track, thumb;
  // The selection's band, where there is a cursor.
  Color cursor;
};

// The scrolling body of a dialog: a column of one-row lines on a plane of its own over the dialog's box, with a
// scrollbar in the box's last column. It is everything a list needs to scroll and be picked from, so ChoiceDialog,
// CommandPalette and a dialog that only shows lines (stocks' attribute dialog) look and act alike: the display's
// smooth scrolling (the lines ease by parts of a row), a draggable scrollbar, a wheel that scrolls without moving
// the selection, and, where there is a cursor (setCursor()), a translucent band that eases to the selected line.
// The dialog draws the lines (drawnRows() of them, from firstLine(), with putText() in the proportional face) and
// passes keys and pointer events as plain values. The selection and the scroll window are a ChoiceList.
class ScrollBody {
 public:
  // The lines there are; the selection starts on `selected`. Everything starts over: the window, the cursor.
  void reset(int count, int selected = 0) {
    list_.reset(count, selected);
    fresh_ = true;
    pressing_ = false;
  }
  // A new number of lines (the selection and the window stay as far as they fit).
  void setCount(int count) {
    if (count != list_.count()) list_.reset(count, list_.selected());
  }
  int count() const { return list_.count(); }

  // Whether there is a selection, drawn as a band over a line. A body with none only scrolls.
  void setCursor(bool cursor) { cursor_ = cursor; }
  // Whether layout() advances the easing by the wall clock (the default), or only tick() does.
  void setClocked(bool clocked) { clocked_ = clocked; }
  void setMouseCursor(MouseCursor cursor) { mouse_cursor_ = cursor; }

  // Places the body over `box` (a plane of the dialog, at `box_row`, `box_col` of the screen), `rows` lines high from
  // row `top` of the box, `cols` wide; the scrollbar is in column `cols` of the box. Erases the body and leaves it
  // with the colours set for the dialog to draw into with plane(): line i of drawnRows() is on row i, and is line
  // firstLine() + i.
  void layout(Plane & screen, Plane & box, int box_row, int box_col, int top, int rows, int cols, const ScrollBodyColors & colors);
  // The same for a body with no cursor.
  void layout(Plane & screen, Plane & box, int box_row, int box_col, int top, int rows, int cols, const Color & background,
              const Color & foreground, const Color & track, const Color & thumb) {
    layout(screen, box, box_row, box_col, top, rows, cols, ScrollBodyColors{background, foreground, track, thumb, background});
  }
  Plane & plane() { return *plane_; }
  void raise() {
    if (plane_) plane_->moveToTop();
  }
  int firstLine() const { return list_.firstShown(); }
  // One more than shown where the display scrolls by parts of a row.
  int drawnRows() const { return rows_ + (list_.smooth() ? 1 : 0); }

  // The selection.
  int selected() const { return list_.selected(); }
  void select(int index) { list_.select(index); }
  void move(int delta) { list_.move(delta); }
  void selectFirst() { list_.home(); }
  void selectLast() { list_.end(); }
  // The line the button is held down on, -1 for none: draw it pressed.
  int pressed() const { return list_.pressed(); }

  // Scrolling without moving the selection, by lines (the wheel is the dialog's to scale).
  void scrollBy(int delta) { list_.scroll(delta, rows_); }
  void home() { scrollBy(-list_.count()); }
  void end() { scrollBy(list_.count()); }
  int page() const { return rows_ > 1 ? rows_ - 1 : 1; }

  // The line under a screen cell, -1 for none (the scrollbar's column counts).
  int lineAt(int row, int col) const;

  // The left button. press() says whether the scrollbar took it, which the body then follows until the release; a
  // press on a line draws it pressed, following the pointer (drag()) while the button is held. `inside_box` is
  // whether the cell is in the dialog, since a press and a release both outside it are for the dialog to cancel.
  bool press(int row, int col, bool inside_box = true);
  bool drag(int row, int col);
  // The release of a scrollbar drag, if there was one.
  bool release(int row, int col);
  bool dragging() const { return list_.thumbDragging(); }
  // The release: a line is chosen by the release over it after a press on it (a release with no press seen before it
  // is a whole click) and becomes the selection; a press and a release both outside the box are OUTSIDE.
  enum class Click { NONE, CHOSEN, OUTSIDE };
  Click click(int row, int col, bool inside_box);

  // Moves the easing on by `seconds`; layout() does it itself unless setClocked(false).
  void tick(double seconds);
  // Whether it is still easing (the scroll or the cursor), so another layout is due.
  bool animating() const;
  void close() { plane_.reset(); }

 private:
  bool onBar(int row, int col) const;
  double target() const { return list_.selected() - list_.offset(); }

  ChoiceList list_;
  std::unique_ptr<Plane> plane_;
  bool cursor_ = false, clocked_ = true, fresh_ = true;
  MouseCursor mouse_cursor_ = MouseCursor::DEFAULT;
  std::chrono::steady_clock::time_point last_tick_;
  // The cursor's row in the body: a whole number except while it eases.
  double cursor_y_ = 0;
  bool pressing_ = false, started_on_line_ = false, started_outside_ = false;
  // Where the body is on the screen, and the lines it shows.
  int row_ = 0, col_ = 0, rows_ = 1, cols_ = 0;
};

}  // namespace cellui
