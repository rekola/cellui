#pragma once

#include <cellui/Widget.h>

#include <algorithm>
#include <initializer_list>

namespace cellui {

// One part of a stack laid out by layoutRows() or layoutColumns(): `size`
// cells along the stack, or with `fill` that many shares of what the sized
// parts leave over; across the stack the whole width (height) less
// `margin_start` and `margin_end`. A part with no widget is a gap.
//
//   layoutRows(0, 0, rows, cols, {{.widget = &tags, .margin_start = 1},
//                                 {.size = 1},  // a gap
//                                 {.widget = &table, .fill = 1},
//                                 {.widget = &status}});
struct LayoutPart {
  Widget * widget = nullptr;
  int size = 1;
  int fill = 0;
  int margin_start = 0, margin_end = 0;
};

namespace detail {

// Sizes and positions along a stack of `extent` cells: the sized parts as asked
// (cut off at the end when they do not fit), the rest shared by `fill`, the last
// filling part taking what rounding leaves.
inline void layoutStack(int extent, std::initializer_list<LayoutPart> parts, bool rows, int top, int left, int across) {
  int fixed = 0, shares = 0;
  const LayoutPart * last_fill = nullptr;
  for (const LayoutPart & part : parts) {
    if (part.fill > 0) {
      shares += part.fill;
      last_fill = &part;
    } else {
      fixed += std::max(0, part.size);
    }
  }
  int spare = std::max(0, extent - fixed), shared = 0, at = 0;
  for (const LayoutPart & part : parts) {
    int size = 0;
    if (part.fill > 0) {
      size = &part == last_fill ? spare - shared : spare * part.fill / shares;
      shared += size;
    } else {
      size = std::max(0, part.size);
    }
    size = std::clamp(size, 0, std::max(0, extent - at));
    if (part.widget) {
      int width = std::max(0, across - part.margin_start - part.margin_end);
      if (rows) {
        part.widget->resize(size, width);
        part.widget->move(top + at, left + part.margin_start);
      } else {
        part.widget->resize(width, size);
        part.widget->move(top + part.margin_start, left + at);
      }
    }
    at += size;
  }
}

}  // namespace detail

// Lays `parts` out top to bottom in the rectangle of `rows` x `cols` cells at
// (top, left) of the widgets' parent plane, each resized and moved.
inline void layoutRows(int top, int left, int rows, int cols, std::initializer_list<LayoutPart> parts) {
  detail::layoutStack(rows, parts, true, top, left, cols);
}

// The same left to right.
inline void layoutColumns(int top, int left, int rows, int cols, std::initializer_list<LayoutPart> parts) {
  detail::layoutStack(cols, parts, false, top, left, rows);
}

}  // namespace cellui
