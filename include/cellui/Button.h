#pragma once

#include <cellui/InputEvents.h>
#include <cellui/Signal.h>
#include <cellui/Utf8.h>
#include <cellui/Widget.h>

#include <string>
#include <vector>

namespace cellui {

// ⓘ-like "circled information source" (U+1F6C8): the info button's glyph. A font without it shows a box or nothing; the
// face is the one place to change that (a different glyph, or a picture).
inline constexpr const char * kInfoGlyph = "\xF0\x9F\x9B\x88";

// What a button shows. A glyph (text) now; a picture drawn where the display has pictures, with the glyph where it has
// none, is one more member here, and nothing that takes a face changes.
struct ButtonFace {
  std::string glyph;

  ButtonFace() = default;
  ButtonFace(std::string g) : glyph(std::move(g)) {}
  ButtonFace(const char * g) : glyph(g) {}
  static ButtonFace info() { return ButtonFace(kInfoGlyph); }

  // The cells it takes.
  int width() const { return utf8::displayWidth(glyph); }
};

// A small button on one row: its face, drawn in the accent colour, pressed while the left button is held on it, focused
// with the theme's highlight. A press and release on it (or Space or Enter while it has the focus) emits `clicked`; a
// release elsewhere does not. It lays out like any widget (resize it to preferredWidth() x 1) and draws itself again
// whenever something changes.
class Button : public Widget {
 public:
  explicit Button(Plane & parent, ButtonFace face = {});

  void setFace(ButtonFace face);
  const ButtonFace & face() const { return face_; }
  int preferredWidth() const { return face_.width(); }
  bool pressed() const { return pressed_; }

  Signal<> clicked;

  void render();

 protected:
  void onResize() override { render(); }

 private:
  bool inside(int screen_row, int screen_col) const;

  ButtonFace face_;
  bool pressed_ = false, focused_ = false;
  std::vector<Connection> connections_;
};

}  // namespace cellui
