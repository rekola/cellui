#pragma once

#include <cellui/Color.h>
#include <cellui/Picture.h>
#include <cellui/StyleProvider.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cellui {

// A sparkline's reading: the level and the held peak as 0..1 fractions of the
// meter (level_meter::fraction()). The bar shows level and nothing else; a warning beside it is a Lamp's
// (Lamp.h).
struct SparklineReading {
  float fraction = 0.0f;
  float peak = 0.0f;  // 0: no peak marker
};

// How a sparkline looks: what it is drawn on, and one colour for the bar in
// place of the meter colours by height (StyleProvider::levelColor()).
// `label` is text laid over the bottom row from the left (a legend for the
// bars): in the picture drawn into it, in cells as text whose background
// approximates the bars under it, so it reads on a silent bar and a full one.
struct SparklineLook {
  Color background;
  std::optional<Color> color;
  std::string label;
};

namespace sparkline_image {

// Vertical bars side by side across a width x height image, one per
// column slot (a cell's two dot columns are two slots), each filled from the
// bottom with a peak line above it; an empty slot stays background. The slots
// are equal whole pixels, any pixels left over split between the edges. Drawn at
// `scale`, the display scale. Returns width*height pixels, row 0 at the
// top, bytes R, G, B, A (all opaque).
std::vector<uint32_t> render(const std::vector<std::optional<SparklineReading>> & columns, const SparklineLook & look, unsigned width,
                             unsigned height, const StyleProvider & styles, float scale = 1.0f);

// The same as a Picture, drawn on a float canvas when `prefer_float` and the backend has one (the top of
// the meter's colours is lit past white), else on an 8-bit one that maps each colour to sRGB.
// `cell_height` is a text row's height in pixels, which sizes the look's label (none is drawn at 0).
Picture renderPicture(const std::vector<std::optional<SparklineReading>> & columns, const SparklineLook & look, unsigned width,
                      unsigned height, const StyleProvider & styles, float scale, bool prefer_float, float cell_height = 0.0f);

}  // namespace sparkline_image

}  // namespace cellui
