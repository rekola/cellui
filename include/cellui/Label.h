#pragma once

#include <cellui/Color.h>
#include <cellui/StyleProvider.h>
#include <cellui/TextStyle.h>
#include <cellui/Widget.h>

#include <string>
#include <variant>

namespace cellui {

// A line of text on a background of its own across the label's whole plane,
// drawn again whenever its text, colours or size change: an info line, a
// caption, a heading. The text is set on the first row, in the display's
// proportional face where it has one, cut with an ellipsis where it does not
// fit (setStyle() changes either). Takes no input.
class Label : public Widget {
 public:
  // A colour: a fixed one, or a role of the theme read at each redraw
  // (&StyleProvider::info_line_fg_color), so a new theme needs no new colour.
  using Paint = std::variant<Color, Color StyleProvider::*>;

  explicit Label(Plane & parent) : Widget(parent) { render(); }

  void setText(std::string text) {
    text_ = std::move(text);
    render();
  }
  const std::string & text() const { return text_; }
  // The text's colour and the background's; the window's text and background by default.
  void setColors(Paint fg, Paint bg) {
    fg_ = fg;
    bg_ = bg;
    render();
  }
  void setStyle(TextStyle style) {
    style_ = style;
    render();
  }

  // Draws it again (a new theme with role colours); every change above does it already.
  void render() {
    if (!hasPlane()) return;
    auto [rows, cols] = getDim();
    erase();
    if (rows <= 0 || cols <= 0) return;
    const StyleProvider & styles = getPlane().getStyles();
    setFgColor(resolve(fg_, styles));
    setBgColor(resolve(bg_, styles));
    fill();
    putText(0, 0, cols, text_, style_);
  }

 protected:
  void onResize() override { render(); }

 private:
  static Color resolve(const Paint & paint, const StyleProvider & styles) {
    if (auto * role = std::get_if<Color StyleProvider::*>(&paint)) return styles.**role;
    return std::get<Color>(paint);
  }

  std::string text_;
  Paint fg_ = &StyleProvider::window_fg_color;
  Paint bg_ = &StyleProvider::window_bg_color;
  TextStyle style_{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::LEFT};
};

}  // namespace cellui
