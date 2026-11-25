#pragma once

#include <cellui/Charts.h>
#include <cellui/term/GlyphMeter.h>

namespace cellui::term {

// The scopes drawn in cells by hand - braille bars, a quadrant or sextant
// heatmap, a quadrant or sextant waveform, the labels in text - for a display
// that shows no pictures. Nothing outside the terminal backend knows these
// glyphs: an application asks its display's Charts and gets whichever version
// it can show.

void drawCellSparkline(Plane & plane, int row, int col, int rows, const std::vector<SparklineReading> & readings, const SparklineLook & look, int clip_top,
                     int clip_bottom, glyph_meter::Glyphs glyphs = glyph_meter::Glyphs::BRAILLE);

// A trace in braille dots, two points to a cell across and four levels to a cell down.
void drawCellTrace(Plane & plane, int row, int col, int rows, int cols, std::span<const float> values, const TraceLook & look, int clip_top,
                   int clip_bottom);

void drawCellWaveformRow(Plane & plane, int row, int col, int width, const std::vector<float> & slices, const WaveformLook & look,
                         bool sextants);

}  // namespace cellui::term
