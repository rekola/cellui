#pragma once

#include <cellui/Color.h>
#include <cellui/Plane.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace cellui {

// Where the thumb of a vertical scrollbar is: `size` cells from `top` in a track of `track` cells, for `content`
// rows of which `view` show, the first of them at `shown` (fractional while the rows ease). Unset when everything
// fits, so no bar is drawn.
struct ScrollbarThumb {
  int top = 0;
  int size = 0;
};

inline std::optional<ScrollbarThumb> scrollbarThumb(int track, int content, int view, double shown) {
  if (track <= 0 || view <= 0 || content <= view) return std::nullopt;
  int size = std::clamp(track * view / content, 1, track);
  int top = static_cast<int>(std::lround((track - size) * shown / std::max(1, content - view)));
  return ScrollbarThumb{std::clamp(top, 0, track - size), size};
}

// Dragging the thumb: begin() on a press at `pos` cells down the track (a press on the thumb keeps where it was
// grabbed; one on the bare track grabs the thumb by its middle, so it jumps there), then offset() for each pointer
// position gives the first row to show. The widgets own the hit test (the scrollbar's column) and the scrolling.
class ScrollbarDrag {
 public:
  bool active() const { return active_; }
  // The offset to scroll to at once, if the press was on the bare track.
  std::optional<int> begin(int track, int content, int view, double shown, int pos) {
    auto thumb = scrollbarThumb(track, content, view, shown);
    if (!thumb) return std::nullopt;
    active_ = true;
    if (pos >= thumb->top && pos < thumb->top + thumb->size) {
      grab_ = pos - thumb->top;
      return std::nullopt;
    }
    grab_ = thumb->size / 2;
    return offset(track, content, view, pos);
  }
  // The first row for a pointer `pos` cells down the track (it may be outside it: the thumb stops at the ends).
  int offset(int track, int content, int view, int pos) const {
    int size = std::clamp(track * view / std::max(1, content), 1, std::max(1, track));
    int room = track - size, span = std::max(0, content - view);
    if (room <= 0 || span <= 0) return 0;
    int top = std::clamp(pos - grab_, 0, room);
    return static_cast<int>(std::lround(static_cast<double>(top) * span / room));
  }
  void end() { active_ = false; }

 private:
  bool active_ = false;
  int grab_ = 0;
};

// Draws it in column `col` of `plane`, the track from row `row` down: a thin line with a heavy thumb on it, the
// look every scrolling list shares. Nothing when the content fits. `glass`: over glass, the track and thumb
// have no background.
inline void drawScrollbar(Plane & plane, int row, int col, int track, int content, int view, double shown, const Color & background,
                          const Color & track_color, const Color & thumb_color, bool glass = false) {
  auto thumb = scrollbarThumb(track, content, view, shown);
  if (!thumb) return;
  // On glass the glyphs have no background of their own (the plane's ink is set for it).
  if (glass) plane.setBgDefault();
  else plane.setBgColor(background);
  plane.setFgColor(track_color);
  for (int y = 0; y < track; y++) plane.putstr(row + y, col, "│");
  plane.setFgColor(thumb_color);
  for (int y = 0; y < thumb->size; y++) plane.putstr(row + thumb->top + y, col, "┃");
}

}  // namespace cellui
