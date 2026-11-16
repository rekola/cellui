#include <cellui/TagBar.h>

#include <cellui/Utf8.h>

#include <algorithm>

namespace cellui {

TagBar::TagBar(Plane & parent) : Widget(parent) {
  setFocusable(true);
  connections_.push_back(on<MouseDown>([this](const MouseDown & e, EventContext &) {
    if (e.button != 1 || e.row != getPosition().first) return Handled::no;
    int tag = tagAt(e.col - getPosition().second);
    if (tag >= 0) select(tag);
    return Handled::yes;
  }));
  connections_.push_back(on<KeyDown>([this](const KeyDown & e, EventContext &) {
    if (e.mods.ctrl || e.mods.alt || e.mods.meta || tags_.empty()) return Handled::no;
    if (e.key == Key::Left) select(std::max(0, selected_ < 0 ? 0 : selected_ - 1));
    else if (e.key == Key::Right) select(std::min(static_cast<int>(tags_.size()) - 1, selected_ + 1));
    else return Handled::no;
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

void TagBar::setTags(std::vector<std::string> tags) {
  tags_ = std::move(tags);
  if (selected_ >= static_cast<int>(tags_.size())) selected_ = -1;
  render();
}

void TagBar::select(int index) {
  if (index < 0 || index >= static_cast<int>(tags_.size())) index = -1;
  if (index == selected_) return;
  selected_ = index;
  render();
  chosen.emit(selected_);
}

std::vector<TagBar::Chip> TagBar::chips() const {
  std::vector<Chip> out;
  int x = 0;
  for (auto & tag : tags_) {
    int w = utf8::displayWidth(tag) + 2;  // a space of padding either side
    out.push_back({x, w});
    x += w + 1;
  }
  return out;
}

int TagBar::preferredWidth() const {
  auto c = chips();
  return c.empty() ? 0 : c.back().x + c.back().width;
}

int TagBar::tagAt(int col) const {
  auto c = chips();
  for (size_t i = 0; i < c.size(); i++) {
    if (col >= c[i].x && col < c[i].x + c[i].width) return static_cast<int>(i);
  }
  return -1;
}

void TagBar::onResize() { render(); }

void TagBar::render() {
  if (!hasPlane()) return;
  auto [rows, cols] = getDim();
  if (rows <= 0 || cols <= 0) return;
  const StyleProvider & styles = getPlane().getStyles();
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
  fill();
  auto c = chips();
  for (size_t i = 0; i < c.size(); i++) {
    if (c[i].x >= cols) break;
    const bool on = static_cast<int>(i) == selected_;
    setBgColor(on ? styles.highlight_bg_color : styles.button_bg_color);
    setFgColor(on ? styles.highlight_fg_color : styles.button_fg_color);
    putstr(0, c[i].x, Plane::cutToCells(" " + tags_[i] + " ", cols - c[i].x, TextOverflow::CLIP), on ? kBoldFace : kPlainFace);
  }
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
}

}  // namespace cellui
