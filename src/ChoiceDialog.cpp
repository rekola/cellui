#include <cellui/ChoiceDialog.h>

#include <cellui/Input.h>
#include <cellui/Scrollbar.h>
#include <cellui/Utf8.h>

#include <algorithm>
#include <cmath>

namespace cellui {

using namespace std;

namespace {

// Wide enough for a long device name plus where it is plugged in, which is
// what tells two of the same make apart.
constexpr int kMaxWidth = 110;
constexpr const char * kHint = "Enter: choose  Esc: cancel";
// The rows above the list (padding, title) and in all around it (also a gap,
// the hint and padding below).
constexpr int kTopRows = 2;
constexpr int kChromeRows = 5;

} // namespace

void ChoiceDialog::open(string title, vector<string> labels, int current) {
  title_ = std::move(title);
  labels_ = std::move(labels);
  current_ = current;
  list_.reset(static_cast<int>(labels_.size()), std::max(0, current));
  pressing_ = false;
  fresh_ = true;
}

bool ChoiceDialog::animating() const {
  if (!isOpen()) return false;
  double target = list_.selected() - list_.offset();
  return list_.animating() || (list_.smooth() && cursor_y_ != target);
}

void ChoiceDialog::close() {
  plane_.reset();
  list_plane_.reset();
  labels_.clear();
  title_.clear();
}

void ChoiceDialog::show(Plane & parent, int screen_rows, int screen_cols) {
  if (labels_.empty()) return;

  // One column of padding each side of the text, and a column beside the
  // longest label for the scroll arrows.
  int widest = utf8::displayWidth(kHint) + 4;
  widest = std::max(widest, utf8::displayWidth(title_) + 4);
  for (auto & label : labels_) widest = std::max(widest, utf8::displayWidth(label) + 8);
  width_ = std::min({widest, kMaxWidth, std::max(10, screen_cols - 2)});
  // A padding row, the title, the list, a gap, the hint and a padding row,
  // leaving a little of the screen around it.
  rows_ = std::min(static_cast<int>(labels_.size()) + kChromeRows, std::max(kChromeRows + 1, screen_rows - 2));

  if (!plane_) {
    plane_ = parent.createChild();
    plane_->setMouseCursor(MouseCursor::DEFAULT);
    plane_->setElevation(elevation_.value_or(plane_->getStyles().theme.elevation.overlay));
    plane_->setCornerRadius(plane_->getStyles().theme.dialog_radius_px);
  }
  plane_->resize(rows_, width_);
  auto & styles = plane_->getStyles();

  auto bg = styles.picker_bg_color, fg = styles.window_fg_color, strong = styles.window_accent_fg_color;
  auto cursor_bg = styles.highlight_bg_color;
  auto set_on = [](Plane & plane, const Color & foreground, const Color & background) {
    plane.setFgColor(foreground);
    plane.setBgColor(background);
  };
  auto set = [&](const Color & foreground, const Color & background) { set_on(*plane_, foreground, background); };

  set(fg, bg);
  for (int row = 0; row < rows_; row++) plane_->putstr(row, 0, string(static_cast<size_t>(width_), ' '));
  int inner = width_ - 2;
  set(strong, bg);
  plane_->putstr(1, 1, utf8::truncateToWidth(title_, inner), kBoldFace);
  set(fg, bg);
  plane_->putstr(rows_ - 2, 1, utf8::truncateToWidth(kHint, inner));

  // The entries scroll smoothly where the display does, under a translucent
  // cursor band that eases to the selection.
  list_.setSmooth(parent.smoothScroll());
  int rows = visibleRows();
  list_.firstVisible(rows);
  double target = list_.selected() - list_.offset();
  if (fresh_) {
    list_.snap();
    cursor_y_ = target;
    fresh_ = false;
    last_tick_ = std::chrono::steady_clock::now();
  }
  auto now = std::chrono::steady_clock::now();
  double seconds = std::min(0.1, std::chrono::duration<double>(now - last_tick_).count());
  last_tick_ = now;
  list_.tick(seconds);
  if (!list_.smooth()) {
    cursor_y_ = target;
  } else {
    cursor_y_ += (target - cursor_y_) * (1.0 - std::exp(-seconds / ScrollAxis::kSettleSeconds));
    if (std::abs(target - cursor_y_) < ScrollAxis::kSnap) cursor_y_ = target;
  }
  cursor_y_ = std::clamp(cursor_y_, 0.0, static_cast<double>(std::max(0, rows - 1)));

  int count = static_cast<int>(labels_.size());
  // The scrollbar rides the right margin of the box, beside the list.
  for (int i = 0; i < rows; i++) plane_->putstr(kTopRows + i, width_ - 1, " ");
  drawScrollbar(*plane_, kTopRows, width_ - 1, rows, count, rows, list_.shown(), bg, styles.theme.text_secondary, strong);

  x_ = (screen_cols - width_) / 2;
  y_ = (screen_rows - rows_) / 2;
  plane_->move(y_, x_);
  plane_->moveToTop();

  if (!list_plane_) {
    list_plane_ = parent.createChild();
    list_plane_->setMouseCursor(MouseCursor::POINTER);
    // Over the box, at its height.
    list_plane_->setElevation(plane_->elevation());
    list_plane_->setCastsShadow(false);
  }
  list_plane_->move(y_ + kTopRows, x_);
  list_plane_->resize(rows, width_ - 1);
  list_plane_->setScrollOffset(list_.fraction(), 0);
  list_plane_->erase();
  set_on(*list_plane_, fg, bg);
  int drawn = rows + (list_.smooth() ? 1 : 0);
  for (int i = 0; i < drawn; i++) {
    int index = list_.firstShown() + i;
    // The entry the button is held down on is drawn pressed.
    bool pressed = index == list_.pressed();
    set_on(*list_plane_, pressed ? strong : fg, pressed ? styles.row_pressed_bg_color : bg);
    list_plane_->putstr(i, 0, string(static_cast<size_t>(width_ - 1), ' '));
    if (index >= count) continue;
    // A marker for the one in use, then the label in the display's proportional face; one that doesn't fit ends in
    // an ellipsis, so it is clear it goes on.
    if (index == current_) list_plane_->putstr(i, 1, "●");
    list_plane_->putText(i, 3, inner - 4, labels_[static_cast<size_t>(index)], TextStyle{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::LEFT});
  }
  // The cursor is not drawn while a scroll of the wheel has left the selection out of view.
  if (target >= 0 && target <= rows - 1) list_plane_->setBand(Band{ cursor_y_, 1.0, 0.0, static_cast<double>(width_ - 1), cursor_bg, 0.45f });
  else list_plane_->clearBand();
  list_plane_->moveToTop();
}

int ChoiceDialog::indexAt(int y, int x) const {
  int row = y - y_ - kTopRows, col = x - x_;
  if (row < 0 || row >= visibleRows() || col < 0 || col >= width_) return -1;
  int index = list_.indexAt(row);
  return index >= 0 && index < list_.count() ? index : -1;
}

// The scrollbar's column, beside the list (nothing where every entry fits).
bool ChoiceDialog::onScrollbar(int y, int x) const {
  int row = y - y_ - kTopRows;
  return x - x_ == width_ - 1 && row >= 0 && row < visibleRows() && list_.count() > visibleRows();
}

// Outside the list the padding and title are inert, and the page behind dismisses.
bool ChoiceDialog::inBox(int y, int x) const { return y >= y_ && y < y_ + rows_ && x - x_ >= 0 && x - x_ < width_; }

ChoiceDialog::Result
ChoiceDialog::press(int y, int x) {
  if (labels_.empty()) return Result::CANCELLED;
  pressing_ = true;
  if (onScrollbar(y, x) && list_.beginThumbDrag(visibleRows(), y - y_ - kTopRows)) {
    started_on_entry_ = started_outside_ = false;
    return Result::NONE;
  }
  int index = indexAt(y, x);
  started_on_entry_ = index >= 0;
  started_outside_ = !inBox(y, x);
  list_.setPressed(index);
  return Result::NONE;
}

ChoiceDialog::Result
ChoiceDialog::drag(int y, int x) {
  if (list_.thumbDragging()) {
    list_.dragThumb(visibleRows(), y - y_ - kTopRows);
    return Result::NONE;
  }
  if (labels_.empty() || !pressing_ || !started_on_entry_) return Result::NONE;
  list_.setPressed(indexAt(y, x));
  return Result::NONE;
}

ChoiceDialog::Result
ChoiceDialog::click(int y, int x) {
  if (labels_.empty()) return Result::CANCELLED;
  if (list_.thumbDragging()) {
    list_.dragThumb(visibleRows(), y - y_ - kTopRows);
    list_.endThumbDrag();
    pressing_ = false;
    return Result::NONE;
  }
  int index = indexAt(y, x);
  bool on_entry = pressing_ ? started_on_entry_ : index >= 0;
  bool outside = pressing_ ? started_outside_ : !inBox(y, x);
  pressing_ = false;
  list_.setPressed(-1);
  if (on_entry && index >= 0) {
    list_.select(index);
    return Result::CHOSEN;
  }
  return outside && !inBox(y, x) ? Result::CANCELLED : Result::NONE;
}

ChoiceDialog::Result
ChoiceDialog::wheel(bool up) {
  if (labels_.empty()) return Result::CANCELLED;
  list_.scroll(up ? -1 : 1, visibleRows());
  return Result::NONE;
}

ChoiceDialog::Result
ChoiceDialog::key(const KeyDown & e) {
  if (labels_.empty()) return Result::CANCELLED;
  auto id = e.key;
  bool ctrl = e.mods.ctrl;
  if (id == Key::Enter) return Result::CHOSEN;
  if (id == Key::Escape || id == 'q' || (ctrl && id == 'g')) return Result::CANCELLED;

  int page = std::max(1, visibleRows() - 1);
  if (id == Key::Up || (ctrl && id == 'p'))
    list_.move(-1);
  else if (id == Key::Down || (ctrl && id == 'n'))
    list_.move(1);
  else if (id == Key::PageUp)
    list_.move(-page);
  else if (id == Key::PageDown)
    list_.move(page);
  else if (id == Key::Home)
    list_.home();
  else if (id == Key::End)
    list_.end();
  return Result::NONE;
}

}  // namespace cellui
