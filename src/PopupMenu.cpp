#include <cellui/PopupMenu.h>

#include <cellui/Utf8.h>

#include <algorithm>

namespace cellui {

PopupMenu::PopupMenu(Widget & root, Plane & screen, EventLoop & loop, std::vector<Item> items)
    : ModalDialog(root, screen, loop), items_(std::move(items)) {
  init();
}

PopupMenu::PopupMenu(Widget & root, Plane & screen, EventLoop & loop, const std::vector<std::string> & labels)
    : ModalDialog(root, screen, loop) {
  for (const std::string & label : labels) items_.push_back({label});
  init();
}

void PopupMenu::init() { selected_ = step(-1, 1); }

int PopupMenu::step(int from, int delta) const {
  const int count = static_cast<int>(items_.size());
  for (int n = 1; n <= count; n++) {
    const int i = ((from + delta * n) % count + count) % count;
    if (choosable(i)) return i;
  }
  return -1;
}

int PopupMenu::indexAt(int row, int col) const {
  if (!inside(row, col)) return -1;
  const int i = row - y_;
  return i < static_cast<int>(items_.size()) ? i : -1;
}

void PopupMenu::select(int i) {
  if (i == selected_) return;
  selected_ = i;
  relayout();
}

void PopupMenu::layout(Plane & screen, int screen_rows, int screen_cols) {
  int widest = 0;
  for (const Item & item : items_) widest = std::max(widest, utf8::displayWidth(item.label));
  rows_ = std::min(static_cast<int>(items_.size()), std::max(1, screen_rows));
  width_ = std::min(widest + 4, screen_cols);  // a cell of margin and a cell of breathing room either side
  if (anchor_row_ < 0) {
    y_ = 0;
    x_ = 0;
  } else {
    y_ = anchor_row_ + 1 + rows_ <= screen_rows ? anchor_row_ + 1 : std::max(0, anchor_row_ - rows_);
    x_ = std::clamp(anchor_col_ + 1 - width_, 0, std::max(0, screen_cols - width_));
  }
  if (!plane_) {
    plane_ = screen.createChild();
    plane_->setMouseCursor(MouseCursor::DEFAULT);
    plane_->setElevation(plane_->getStyles().theme.elevation.overlay);
    plane_->setCornerRadius(plane_->getStyles().theme.dialog_radius_px / 2);
  }
  plane_->resize(rows_, width_);
  const StyleProvider & styles = plane_->getStyles();
  plane_->setBorder(styles.window_fg_color, 1.0, 0.3f);
  for (int i = 0; i < rows_; i++) {
    const Item & item = items_[static_cast<size_t>(i)];
    const bool on = i == selected_;
    plane_->setFgColor(on ? styles.highlight_fg_color : item.enabled ? styles.window_fg_color : styles.theme.text_secondary);
    // Glass where the display blurs, else the opaque picker colour; the selected row is opaque either way.
    if (on || !plane_->setFrosted(styles.picker_bg_color)) {
      plane_->clearFrosted();
      plane_->setBgColor(on ? styles.highlight_bg_color : styles.picker_bg_color);
    }
    plane_->putstr(i, 0, std::string(static_cast<size_t>(width_), ' '));
    if (item.separator) {
      plane_->setFgColor(styles.theme.text_secondary);
      std::string line;
      for (int c = 1; c < width_ - 1; c++) line += "\xE2\x94\x80";  // a light horizontal line
      plane_->putstr(i, 1, line);
    } else {
      plane_->putText(i, 2, width_ - 4, item.label, TextStyle{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::LEFT});
    }
  }
  auto [origin_row, origin_col] = screen.childOrigin();
  plane_->move(origin_row + y_, origin_col + x_);
  plane_->moveToTop();
}

void PopupMenu::keyDown(const KeyDown & e) {
  const bool ctrl = e.mods.ctrl;
  const int id = e.key;
  if (id == Key::Escape || id == 'q' || (ctrl && id == 'g')) {
    done(kClosed);
  } else if ((id == Key::Enter || id == ' ') && !ctrl) {
    if (choosable(selected_)) done(selected_);
  } else if (id == Key::Up || (ctrl && id == 'p')) {
    select(step(selected_ < 0 ? 0 : selected_, -1));
  } else if (id == Key::Down || (ctrl && id == 'n')) {
    select(step(selected_ < 0 ? -1 : selected_, 1));
  } else if (id == Key::Home) {
    select(step(-1, 1));
  } else if (id == Key::End) {
    select(step(static_cast<int>(items_.size()), -1));
  }
}

void PopupMenu::mouseDown(const MouseDown & e) {
  pressed_inside_ = inside(e.row, e.col);
  if (!pressed_inside_) done(kClosed);  // a click away closes it, and goes no further
  else if (const int i = indexAt(e.row, e.col); choosable(i)) select(i);
}

void PopupMenu::mouseMove(const MouseMove & e) {
  if (const int i = indexAt(e.row, e.col); choosable(i)) select(i);
}

void PopupMenu::mouseUp(const MouseUp & e) {
  if (e.button != 1) return;
  // The release over an item chooses it, whether or not the press was on it (the menu may have opened under the press that made it).
  if (const int i = indexAt(e.row, e.col); choosable(i)) done(i);
  else if (!inside(e.row, e.col) && pressed_inside_) pressed_inside_ = false;
}

void PopupMenu::wheel(const Wheel & e) {
  if (e.dy > 0) select(step(selected_ < 0 ? 0 : selected_, -1));
  else if (e.dy < 0) select(step(selected_ < 0 ? -1 : selected_, 1));
}

}  // namespace cellui
