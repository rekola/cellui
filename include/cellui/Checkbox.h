#pragma once

#include <cellui/Color.h>
#include <cellui/InputEvents.h>
#include <cellui/Input.h>
#include <cellui/StyleProvider.h>
#include <cellui/Utf8.h>
#include <cellui/Widget.h>

#include <functional>
#include <string>

namespace cellui {

// A check box: a box, "[x]" when on and "[ ]" when off, and a label after it, on one row. It owns no state
// (get/set callbacks, like SpinBox), so it can show any flag. It takes the keyboard focus (focusable, so the
// dispatcher's focus order and a click reach it); focused it is drawn with the theme's highlight, and Space or
// Enter toggles it. A press of the left button on it toggles it as well. A host that does not use the
// dispatcher's focus (a panel that routes its own keys) sets the look with setFocused() and passes keys to
// key().
class Checkbox : public Widget {
 public:
  Checkbox(Plane & parent, std::string label, std::function<bool()> get_value, std::function<void(bool)> set_value)
      : Widget(parent), label_(std::move(label)), get_value_(std::move(get_value)), set_value_(std::move(set_value)) {
    setFocusable(true);
    connections_.push_back(on<FocusIn>([this](const FocusIn &, EventContext &) { setFocused(true); return Handled::no; }));
    connections_.push_back(on<FocusOut>([this](const FocusOut &, EventContext &) { setFocused(false); return Handled::no; }));
    connections_.push_back(on<KeyDown>([this](const KeyDown & e, EventContext &) { return key(e) ? Handled::yes : Handled::no; }));
    connections_.push_back(on<MouseDown>([this](const MouseDown & e, EventContext &) { return e.button == 1 && toggle() ? Handled::yes : Handled::no; }));
  }

  // The columns the box and its label need.
  int preferredWidth() const { return kBoxWidth + 1 + utf8::displayWidth(label_); }

  bool checked() const { return get_value_(); }
  bool focused() const { return focused_; }
  void setFocused(bool focused) { focused_ = focused; }

  // Flips the flag; always true (the event was used).
  bool toggle() {
    set_value_(!get_value_());
    return true;
  }

  // Space or Enter, without modifiers, toggles. Returns whether the key was used.
  bool key(const KeyDown & e) {
    if (e.mods.ctrl || e.mods.alt || e.mods.meta || e.mods.shift) return false;
    if (e.key != Key::Space && e.key != Key::Enter) return false;
    if (!e.repeat) toggle();
    return true;
  }

  // Draws the box and the label on the widget's row. Returns false when nothing changed since the last call
  // (unless `refresh`).
  bool render(const StyleProvider & styles, bool refresh = false) {
    bool on = get_value_();
    if (!refresh && on == drawn_value_ && focused_ == drawn_focus_ && drawn_) return false;
    auto cols = getDim().second;
    setFgColor(focused_ ? styles.highlight_fg_color : styles.window_fg_color);
    setBgColor(focused_ ? styles.highlight_bg_color : styles.picker_bg_color);
    putstr(0, 0, std::string(static_cast<size_t>(std::max(0, cols)), ' '));
    putstr(0, 0, std::string(on ? "[x] " : "[ ] ") + label_);
    drawn_ = true;
    drawn_value_ = on;
    drawn_focus_ = focused_;
    return true;
  }

 private:
  static constexpr int kBoxWidth = 3;
  std::string label_;
  std::function<bool()> get_value_;
  std::function<void(bool)> set_value_;
  bool focused_ = false;
  bool drawn_ = false;
  bool drawn_value_ = false;
  bool drawn_focus_ = false;
  std::vector<Connection> connections_;
};

}  // namespace cellui
