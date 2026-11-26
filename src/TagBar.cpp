#include <cellui/TagBar.h>

#include <cellui/Utf8.h>

#include <algorithm>
#include <cstdlib>

namespace cellui {

TagBar::TagBar(Plane & parent) : Widget(parent) {
  setFocusable(true);
  connections_.push_back(on<MouseDown>([this](const MouseDown & e, EventContext &) {
    if (e.button != 1) return Handled::no;
    int tag = tagAt(e.row - getPosition().first, e.col - getPosition().second);
    if (tag >= 0) select(tag);
    return Handled::yes;
  }));
  connections_.push_back(on<KeyDown>([this](const KeyDown & e, EventContext &) {
    if (e.mods.ctrl || e.mods.alt || e.mods.meta || tags_.empty()) return Handled::no;
    if (e.key == Key::Left) select(std::max(0, selected_ < 0 ? 0 : selected_ - 1));
    else if (e.key == Key::Right) select(std::min(static_cast<int>(tags_.size()) - 1, selected_ + 1));
    else if (e.key == Key::Up || e.key == Key::Down) {
      const int beside = tagBeside(e.key == Key::Up ? -1 : 1);
      if (beside < 0) return Handled::no;  // the first or the last line: the focus goes on as it would
      select(beside);
    } else return Handled::no;
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

std::vector<TagBar::Chip> TagBar::chips(int cols) const {
  std::vector<Chip> out;
  int row = 0, x = 0;
  for (auto & tag : tags_) {
    int w = utf8::displayWidth(tag) + 2;  // a space of padding either side
    if (cols > 0 && x > 0 && x + w > cols) {  // does not fit after the others: the next line
      row++;
      x = 0;
    }
    out.push_back({row, x, w});
    x += w + 1;
  }
  return out;
}

int TagBar::preferredWidth() const {
  auto c = chips(0);
  return c.empty() ? 0 : c.back().x + c.back().width;
}

int TagBar::lines(int cols) const {
  auto c = chips(cols);
  return c.empty() ? 1 : c.back().row + 1;
}

int TagBar::tagAt(int col) const { return tagAt(0, col); }

int TagBar::tagAt(int row, int col) const {
  auto c = chips();
  for (size_t i = 0; i < c.size(); i++) {
    if (row == c[i].row && col >= c[i].x && col < c[i].x + c[i].width) return static_cast<int>(i);
  }
  return -1;
}

int TagBar::tagBeside(int delta) const {
  const auto c = chips();
  if (selected_ < 0 || selected_ >= static_cast<int>(c.size())) return -1;
  const int row = c[static_cast<size_t>(selected_)].row + delta, x = c[static_cast<size_t>(selected_)].x;
  int best = -1;
  for (size_t i = 0; i < c.size(); i++)
    if (c[i].row == row && (best < 0 || std::abs(c[i].x - x) < std::abs(c[static_cast<size_t>(best)].x - x))) best = static_cast<int>(i);
  return best;
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
  // A plane for each tag, so each can have its own border.
  while (chip_planes_.size() > tags_.size()) chip_planes_.pop_back();
  while (chip_planes_.size() < tags_.size()) {
    chip_planes_.push_back(getPlane().createChild());
    chip_planes_.back()->setMouseCursor(MouseCursor::POINTER);
  }
  auto c = chips();
  for (size_t i = 0; i < c.size(); i++) {
    Plane & chip = *chip_planes_[i];
    const bool shown = c[i].x < cols && c[i].row < rows;
    chip.setVisible(shown);
    if (!shown) continue;
    chip.resize(1, c[i].width);
    // Where the chip goes is given in the bar's own columns; a flat tree wants the root's.
    auto [origin_row, origin_col] = getPlane().childOrigin();
    chip.move(origin_row + c[i].row, origin_col + c[i].x);
    const bool on = static_cast<int>(i) == selected_;
    // The chosen tag is filled with the accent; the others are dark with a lighter border, so both read as tags.
    chip.setBgColor(on ? styles.highlight_bg_color : styles.heading_bg_color);
    chip.setFgColor(on ? styles.highlight_fg_color : styles.window_accent_fg_color);
    chip.putstr(0, 0, std::string(static_cast<size_t>(c[i].width), ' '));
    chip.putstr(0, 0, " " + tags_[i] + " ", on ? kBoldFace : kPlainFace);
    // A one pixel border inside the tag's rectangle with soft corners.
    chip.setCornerRadius(3.0);
    chip.setBorder(on ? styles.highlight_bg_color.blend(0.45f, Color(255, 255, 255)) : styles.window_fg_color, 1.0, on ? 0.9f : 0.55f);
  }
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
}

}  // namespace cellui
