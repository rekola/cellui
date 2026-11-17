#include <cellui/StageStats.h>
#include <cellui/TraceImage.h>

#include <canvas/Context.h>
#include "NativeContext.h"

#include <algorithm>
#include <cmath>

namespace cellui {

namespace trace_image {

namespace {

// A colour as canvas takes it from a linear Rec.2020 context: the components as they are.
canvas::Color toCanvas(const Color & c, float alpha = 1.0f) { return canvas::Color(c.r(), c.g(), c.b(), alpha); }

}  // namespace

std::vector<float> resample(const std::vector<float> & values, size_t count) {
  std::vector<float> finite;
  finite.reserve(values.size());
  for (float v : values) {
    if (std::isfinite(v)) finite.push_back(v);
  }
  std::vector<float> out;
  if (finite.empty() || count == 0) return out;
  auto [lo, hi] = std::minmax_element(finite.begin(), finite.end());
  const float low = *lo, span = *hi - *lo;
  out.reserve(count);
  for (size_t i = 0; i < count; i++) {
    // The position in the series of point i: the first point is the first
    // value and the last point the last.
    double at = count == 1 ? 0.0 : static_cast<double>(i) * static_cast<double>(finite.size() - 1) / static_cast<double>(count - 1);
    size_t k = static_cast<size_t>(at);
    double t = at - static_cast<double>(k);
    float v = k + 1 < finite.size() ? static_cast<float>(finite[k] * (1.0 - t) + finite[k + 1] * t) : finite[k];
    out.push_back(span > 0.0f ? (v - low) / span : 0.5f);
  }
  return out;
}

Picture renderPicture(const std::vector<float> & values, const TraceLook & look, unsigned width, unsigned height, float scale, bool prefer_float) {
  StageTimer timer(Stage::PICTURE_RENDER);
  Picture blank;
  blank.width = static_cast<int>(width);
  blank.height = static_cast<int>(height);
  blank.pixels.assign(static_cast<size_t>(width) * height, 0);
  if (width == 0 || height == 0) return blank;

  scale = std::max(1.0f, scale);
  LinearContext drawn = nativeLinearContext(width, height, scale, prefer_float);
  if (!drawn.context) return blank;
  canvas::Context & ctx = *drawn.context;
  const float w = static_cast<float>(width) / scale, h = static_cast<float>(height) / scale;
  ctx.fillStyle = toCanvas(look.background);
  ctx.fillRect(0, 0, std::ceil(w), std::ceil(h));

  // One point per pixel column, a little in from the top and bottom edges so
  // the stroke is not cut off.
  std::vector<float> levels = resample(values, width);
  if (!levels.empty()) {
    const float inset = std::min(2.0f, h / 4.0f);
    auto x_at = [&](size_t x) { return (static_cast<double>(x) + 0.5) / scale; };
    auto y_at = [&](size_t x) { return h - (levels[x] * (h - 2.0f * inset) + inset); };
    if (look.fill) {
      ctx.beginPath().moveTo(x_at(0), h);
      for (size_t x = 0; x < levels.size(); x++) ctx.lineTo(x_at(x), y_at(x));
      ctx.lineTo(x_at(levels.size() - 1), h).closePath();
      // Fades in from nothing at the bottom, so the line's body is the brightest part of the fill.
      auto fill = ctx.createLinearGradient(0, h, 0, 0);
      fill.addColorStop(0.0f, toCanvas(*look.fill, 0.03f));
      fill.addColorStop(1.0f, toCanvas(*look.fill, 0.30f));
      ctx.fillStyle = fill;
      ctx.fill();
    }
    ctx.beginPath().moveTo(x_at(0), y_at(0));
    for (size_t x = 1; x < levels.size(); x++) ctx.lineTo(x_at(x), y_at(x));
    auto stroke = ctx.createLinearGradient(0, h, 0, 0);
    // The light past white rises on a curve, so the gradient is more than its two ends.
    for (int i = 0; i <= 8; i++) {
      float t = static_cast<float>(i) / 8.0f;
      stroke.addColorStop(t, toCanvas(lineColorAt(look, t, true)));
    }
    ctx.strokeStyle = stroke;
    ctx.lineWidth = 1.5f;
    ctx.stroke();
  }

  Picture out = readPicture(drawn, width, height, scale);
  return out.width ? out : blank;
}

std::vector<uint32_t> render(const std::vector<float> & values, const TraceLook & look, unsigned width, unsigned height, float scale) {
  return renderPicture(values, look, width, height, scale, false).pixels;
}

}  // namespace trace_image

}  // namespace cellui
