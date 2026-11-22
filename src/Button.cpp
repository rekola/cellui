#include <cellui/Button.h>

#include <cellui/Input.h>

namespace cellui {

Button::Button(Plane & parent, ButtonFace face) : Widget(parent), face_(std::move(face)) {
  setFocusable(true);
  connections_.push_back(on<MouseDown>([this](const MouseDown & e, EventContext &) {
    if (e.button != 1) return Handled::no;
    pressed_ = true;
    render();
    return Handled::yes;
  }));
  // The pointer is grabbed from press to release: moving off it shows it released, moving back presses it again.
  connections_.push_back(on<MouseMove>([this](const MouseMove & e, EventContext &) {
    if (!(e.buttons & 1)) return Handled::no;
    const bool over = inside(e.row, e.col);
    if (over != pressed_) {
      pressed_ = over;
      render();
    }
    return Handled::yes;
  }));
  connections_.push_back(on<MouseUp>([this](const MouseUp & e, EventContext &) {
    if (e.button != 1 || !pressed_) return Handled::no;
    pressed_ = false;
    render();
    if (inside(e.row, e.col)) clicked.emit();
    return Handled::yes;
  }));
  connections_.push_back(on<KeyDown>([this](const KeyDown & e, EventContext &) {
    if (e.mods.ctrl || e.mods.alt || e.mods.meta || e.mods.shift) return Handled::no;
    if (e.key != Key::Space && e.key != Key::Enter) return Handled::no;
    if (!e.repeat) clicked.emit();
    return Handled::yes;
  }));
  connections_.push_back(on<FocusIn>([this](const FocusIn &, EventContext &) {
    focused_ = true;
    render();
    return Handled::no;
  }));
  connections_.push_back(on<FocusOut>([this](const FocusOut &, EventContext &) {
    focused_ = false;
    render();
    return Handled::no;
  }));
}

void Button::setFace(ButtonFace face) {
  face_ = std::move(face);
  render();
}

bool Button::inside(int screen_row, int screen_col) const {
  auto [py, px] = getPosition();
  auto [rows, cols] = getDim();
  return screen_row >= py && screen_row < py + rows && screen_col >= px && screen_col < px + cols;
}

void Button::render() {
  if (!hasPlane()) return;
  auto [rows, cols] = getDim();
  if (rows <= 0 || cols <= 0) return;
  const StyleProvider & styles = getPlane().getStyles();
  getPlane().erase();
  setFgColor(pressed_ ? styles.window_accent_fg_color : focused_ ? styles.highlight_fg_color : styles.window_accent_fg_color);
  setBgColor(pressed_ ? styles.button_pressed_bg_color : focused_ ? styles.highlight_bg_color : styles.window_bg_color);
  fill();
  putstr(rows / 2, 0, face_.glyph);
}

}  // namespace cellui
