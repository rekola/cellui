#include <cellui/ChoiceDialog.h>

#include <cellui/Input.h>
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
  body_.setCursor(true);
  body_.setMouseCursor(MouseCursor::POINTER);
  body_.reset(static_cast<int>(labels_.size()), std::max(0, current));
}

bool ChoiceDialog::animating() const {
  return isOpen() && body_.animating();
}

void ChoiceDialog::close() {
  plane_.reset();
  body_.close();
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

  int rows = visibleRows();
  int count = static_cast<int>(labels_.size());
  x_ = (screen_cols - width_) / 2;
  y_ = (screen_rows - rows_) / 2;
  plane_->move(y_, x_);

  // The entries scroll smoothly where the display does, under a translucent cursor band that eases to the selection.
  body_.layout(parent, *plane_, y_, x_, kTopRows, rows, width_ - 1,
               ScrollBodyColors{bg, fg, styles.theme.text_secondary, strong, cursor_bg});
  Plane & list = body_.plane();
  for (int i = 0; i < body_.drawnRows(); i++) {
    int index = body_.firstLine() + i;
    // The entry the button is held down on is drawn pressed.
    bool pressed = index == body_.pressed();
    set_on(list, pressed ? strong : fg, pressed ? styles.row_pressed_bg_color : bg);
    list.putstr(i, 0, string(static_cast<size_t>(width_ - 1), ' '));
    if (index >= count) continue;
    // A marker for the one in use, then the label in the display's proportional face; one that doesn't fit ends in
    // an ellipsis, so it is clear it goes on.
    if (index == current_) list.putstr(i, 1, "●");
    list.putText(i, 3, inner - 4, labels_[static_cast<size_t>(index)], TextStyle{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::LEFT});
  }
  plane_->moveToTop();
  body_.raise();
}

// Outside the list the padding and title are inert, and the page behind dismisses.
bool ChoiceDialog::inBox(int y, int x) const { return y >= y_ && y < y_ + rows_ && x - x_ >= 0 && x - x_ < width_; }

ChoiceDialog::Result
ChoiceDialog::press(int y, int x) {
  if (labels_.empty()) return Result::CANCELLED;
  body_.press(y, x, inBox(y, x));
  return Result::NONE;
}

ChoiceDialog::Result
ChoiceDialog::drag(int y, int x) {
  body_.drag(y, x);
  return Result::NONE;
}

ChoiceDialog::Result
ChoiceDialog::click(int y, int x) {
  if (labels_.empty()) return Result::CANCELLED;
  switch (body_.click(y, x, inBox(y, x))) {
    case ScrollBody::Click::CHOSEN: return Result::CHOSEN;
    case ScrollBody::Click::OUTSIDE: return Result::CANCELLED;
    case ScrollBody::Click::NONE: break;
  }
  return Result::NONE;
}

ChoiceDialog::Result
ChoiceDialog::wheel(bool up) {
  if (labels_.empty()) return Result::CANCELLED;
  body_.scrollBy(up ? -1 : 1);
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
    body_.move(-1);
  else if (id == Key::Down || (ctrl && id == 'n'))
    body_.move(1);
  else if (id == Key::PageUp)
    body_.move(-page);
  else if (id == Key::PageDown)
    body_.move(page);
  else if (id == Key::Home)
    body_.selectFirst();
  else if (id == Key::End)
    body_.selectLast();
  return Result::NONE;
}

}  // namespace cellui
