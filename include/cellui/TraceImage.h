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
//
// The line is a gradient up its box: `line` blended in from the background at
// the bottom, and from about a third of the way up lit past white, `hdr_gain`
// times its light at the top, so on a display that shows light past white (a
// float picture, DisplayInfo::hdr_pictures) the high part of the series glows
// in its own colour; elsewhere it is the colour washed toward white. The
// version drawn in cells has no light past white and keeps to the colour.
struct TraceLook {
  Color background;
  Color line;
  std::optional<Color> fill;
  float hdr_gain = 4.0f;  // 0: no light past white
};

namespace trace_image {

// The line's colour `t` of the way up its box (0 the bottom, 1 the top). With
// `hdr` the top is lit past white by look.hdr_gain.
inline Color lineColorAt(const TraceLook & look, float t, bool hdr) {
  Color base = look.background.blend(0.55f + 0.45f * t, look.line);
  return hdr ? base * StyleProvider::litGain(t, 0.35f, look.hdr_gain) : base;
}

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
