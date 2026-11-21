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

// Draws it in column `col` of `plane`, the track from row `row` down: a thin line with a heavy thumb on it, the
// look every scrolling list shares. Nothing when the content fits.
inline void drawScrollbar(Plane & plane, int row, int col, int track, int content, int view, double shown, const Color & background,
                          const Color & track_color, const Color & thumb_color) {
  auto thumb = scrollbarThumb(track, content, view, shown);
  if (!thumb) return;
  plane.setBgColor(background);
  plane.setFgColor(track_color);
  for (int y = 0; y < track; y++) plane.putstr(row + y, col, "│");
  plane.setFgColor(thumb_color);
  for (int y = 0; y < thumb->size; y++) plane.putstr(row + thumb->top + y, col, "┃");
}

}  // namespace cellui
