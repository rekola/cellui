#pragma once

#include <cellui/Color.h>
#include <cellui/Picture.h>
#include <cellui/StyleProvider.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace cellui {

// How a trace looks: what it is drawn on, the line's colour, and the colour
// the area under the line is faintly filled with (none: only the line). A
// trace is a series drawn left to right as a line - a price history, a load
// curve - scaled to fill its box from its lowest value to its highest, with
// nothing said about what the values are.
struct TraceLook {
  Color background;
  Color line;
  std::optional<Color> fill;
};

namespace trace_image {

// `values` as `count` evenly spaced points from 0 (the lowest value) to 1
// (the highest), interpolated between neighbours. Values that are not finite
// are left out first; a series with no value gives no points, one with a
// single value (or all equal) a flat line in the middle.
std::vector<float> resample(const std::vector<float> & values, size_t count);

// The trace as pixels, `width` x `height`, at `scale`, the display scale
// (line widths are in logical units). Drawn on a float canvas when
// `prefer_float` and the backend has one, else on an 8-bit one that maps each
// colour to sRGB.
Picture renderPicture(const std::vector<float> & values, const TraceLook & look, unsigned width, unsigned height, float scale, bool prefer_float);

std::vector<uint32_t> render(const std::vector<float> & values, const TraceLook & look, unsigned width, unsigned height, float scale = 1.0f);

}  // namespace trace_image

}  // namespace cellui
