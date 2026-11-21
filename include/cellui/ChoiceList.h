#pragma once

#include <cellui/Scrollbar.h>
#include <cellui/Viewport.h>

#include <algorithm>
#include <optional>

// The selection and scroll position of a list a person picks one entry from -
// the part of a choice dialog that doesn't depend on any toolkit. Movement
// stops at the ends rather than wrapping, since the first and last entries of
// a long list are the ones people overshoot to.
namespace cellui {

class ChoiceList {
 public:
  void reset(int count, int selected) {
    count_ = std::max(0, count);
    window_ = ScrollAxis();
    pressed_ = -1;
    selected_ = 0;
    select(selected);
  }

  int count() const { return count_; }
  int selected() const { return selected_; }

  // Moving the selection brings the window back to it.
  void select(int index) {
    selected_ = count_ == 0 ? 0 : std::clamp(index, 0, count_ - 1);
    detached_ = false;
  }
  void move(int delta) { select(selected_ + delta); }
  void home() { select(0); }
  void end() { select(count_ - 1); }

  // Index of the first entry shown in a window of `rows` rows. Scrolls only
  // as far as needed to keep the selection in view, so the window doesn't
  // jump while the selection moves inside it.
  // Unless the window was scrolled by hand (scroll()) and the selection has not moved since: then it stays where it
  // is, the selection out of view if that is where it went.
  int firstVisible(int rows) {
    window_.setExtent(count_, std::max(1, rows));
    if (!detached_) window_.reveal(selected_);
    return window_.offset();
  }

  // Scrolls the window without moving the selection (the mouse wheel), which may end up out of view: a list is
  // browsed without choosing. The window stays there until the selection moves.
  void scroll(int delta, int rows) {
    window_.setExtent(count_, std::max(1, rows));
    window_.scrollBy(delta);
    detached_ = true;
  }
  bool detached() const { return detached_; }

  // The entry the pointer is held down on (-1 for none), drawn pressed. Choosing it is for the release.
  int pressed() const { return pressed_; }
  void setPressed(int index) { pressed_ = index >= 0 && index < count_ ? index : -1; }

  // Where the display scrolls smoothly the window follows its offset with an
  // animation (see ScrollAxis): firstShown() and fraction() say where it is.
  void setSmooth(bool smooth) { window_.setSmooth(smooth); }
  bool smooth() const { return window_.smooth(); }
  // The window jumps to its offset (a list just opened).
  void snap() {
    bool smooth = window_.smooth();
    window_.setSmooth(false);
    window_.setSmooth(smooth);
  }
  void tick(double seconds) { window_.tick(seconds); }
  bool animating() const { return window_.animating(); }
  int offset() const { return window_.offset(); }
  int firstShown() const { return window_.firstShown(); }
  // Where the window is now, in entries (fractional while it eases).
  double shown() const { return window_.shown(); }
  double fraction() const { return window_.fraction(); }
  // Where the scrollbar's thumb is in a track of `track` cells beside a window of `rows` rows: unset when every
  // entry fits. Draw it with drawScrollbar() (Scrollbar.h), over the rows' own column.
  std::optional<ScrollbarThumb> thumb(int rows, int track) const { return scrollbarThumb(track, count_, rows, window_.shown()); }
  // The entry under row `row` of the window, as drawn now.
  int indexAt(int row) const { return window_.indexAt(row); }

 private:
  int count_ = 0;
  int selected_ = 0;
  int pressed_ = -1;
  bool detached_ = false;
  ScrollAxis window_;
};

}  // namespace cellui
