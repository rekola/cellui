#include <cellui/FormModal.h>

#include <cellui/Input.h>
#include <cellui/NumberParse.h>
#include <cellui/Utf8.h>

#include <algorithm>
#include <cmath>

namespace cellui {

namespace {

constexpr int kMinWidth = 60;  // an error and a summary are a line each: room for a sentence
constexpr int kMaxWidth = 80;
constexpr int kFieldWidth = 30;
constexpr const char * kOk = " OK ";
constexpr const char * kCancel = " Cancel ";
constexpr const char * kHint = "Tab: next  Enter: OK  Esc: cancel";

std::string trimmed(const std::string & s) {
  size_t a = 0, b = s.size();
  while (a < b && s[a] == ' ') a++;
  while (b > a && s[b - 1] == ' ') b--;
  return s.substr(a, b - a);
}

bool digits(const std::string & s, size_t from, size_t count) {
  if (from + count > s.size()) return false;
  for (size_t i = from; i < from + count; i++)
    if (s[i] < '0' || s[i] > '9') return false;
  return true;
}

int daysIn(int year, int month) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return days[month - 1] + (month == 2 && leap ? 1 : 0);
}

}  // namespace

std::optional<double> parseFormNumber(const std::string & text) {
  std::string s = trimmed(text);
  if (s.empty()) return std::nullopt;
  // A comma is a decimal point where there is no point (a Finnish "4,20"); two separators are no number.
  const size_t comma = s.find(',');
  if (comma != std::string::npos) {
    if (s.find('.') != std::string::npos || s.find(',', comma + 1) != std::string::npos) return std::nullopt;
    s[comma] = '.';
  }
  size_t start = 0;
  if (s[0] == '+') start = 1;  // from_chars takes a minus but not a plus
  if (start >= s.size()) return std::nullopt;
  double v = 0;
  const auto result = parseFloat(s.data() + start, s.data() + s.size(), v);
  if (result.ec != std::errc() || result.ptr != s.data() + s.size() || !std::isfinite(v)) return std::nullopt;
  return v;
}

std::optional<FormDate> parseFormDate(const std::string & text) {
  const std::string s = trimmed(text);
  if (s.size() != 10 || !digits(s, 0, 4) || s[4] != '-' || !digits(s, 5, 2) || s[7] != '-' || !digits(s, 8, 2)) return std::nullopt;
  FormDate d;
  d.year = std::stoi(s.substr(0, 4));
  d.month = std::stoi(s.substr(5, 2));
  d.day = std::stoi(s.substr(8, 2));
  if (d.year < 1 || d.month < 1 || d.month > 12 || d.day < 1 || d.day > daysIn(d.year, d.month)) return std::nullopt;
  return d;
}

FormModal::FormModal(Widget & root, Plane & screen, EventLoop & loop, std::string title, std::vector<FormField> fields)
    : ModalDialog(root, screen, loop), title_(std::move(title)), fields_(std::move(fields)) {
  editors_.resize(fields_.size());
  for (size_t i = 0; i < fields_.size(); i++) editors_[i].setText(fields_[i].initial);
  field_spots_.resize(fields_.size());
}

void FormModal::setSummary(std::function<std::string(const std::vector<std::string> &)> summary) {
  summary_fn_ = std::move(summary);
  changed();
}

void FormModal::setCheck(std::function<std::string(const std::vector<std::string> &)> check) {
  check_fn_ = std::move(check);
  changed();
}

std::string FormModal::text(size_t field) const { return field < editors_.size() ? editors_[field].text() : std::string(); }

void FormModal::setText(size_t field, const std::string & text) {
  if (field >= editors_.size()) return;
  editors_[field].setText(text);
  changed();
}

std::vector<std::string> FormModal::texts() const {
  std::vector<std::string> out;
  for (const LineEditor & e : editors_) out.push_back(e.text());
  return out;
}

std::optional<double> FormModal::number(size_t field) const {
  if (field >= fields_.size() || fields_[field].kind != FieldKind::NUMBER) return std::nullopt;
  return parseFormNumber(text(field));
}

std::optional<FormDate> FormModal::date(size_t field) const {
  if (field >= fields_.size() || fields_[field].kind != FieldKind::DATE) return std::nullopt;
  return parseFormDate(text(field));
}

void FormModal::setFocus(size_t focus) {
  focus_ = std::min(focus, fields_.size() + 1);
  changed();
}

std::string FormModal::fieldError(size_t i) const {
  if (i >= fields_.size()) return {};
  const FormField & f = fields_[i];
  const std::string t = text(i);
  if (trimmed(t).empty()) return f.required ? f.label + " is required" : std::string();
  if (f.kind == FieldKind::NUMBER && !parseFormNumber(t)) return f.label + " is not a number";
  if (f.kind == FieldKind::DATE && !parseFormDate(t)) return f.label + " is not a date (YYYY-MM-DD)";
  return f.check ? f.check(t) : std::string();
}

std::string FormModal::formError() const {
  for (size_t i = 0; i < fields_.size(); i++)
    if (!fieldError(i).empty()) return {};  // the fields' own errors come first
  return check_fn_ ? check_fn_(texts()) : std::string();
}

bool FormModal::valid() const {
  for (size_t i = 0; i < fields_.size(); i++)
    if (!fieldError(i).empty()) return false;
  return formError().empty();
}

// The error line: the form's own error (live), the focused field's while it has text that is wrong, and after a refused Enter the
// first wrong field's.
void FormModal::changed() {
  summary_ = summary_fn_ ? summary_fn_(texts()) : std::string();
  error_.clear();
  if (focus_ < fields_.size() && !trimmed(text(focus_)).empty()) error_ = fieldError(focus_);
  if (error_.empty() && attempted_)
    for (size_t i = 0; i < fields_.size() && error_.empty(); i++) error_ = fieldError(i);
  if (error_.empty()) error_ = formError();
  relayout();
}

void FormModal::accept() {
  attempted_ = true;
  for (size_t i = 0; i < fields_.size(); i++)
    if (!fieldError(i).empty()) {
      focus_ = i;  // the first wrong one, with its error
      changed();
      return;
    }
  if (!formError().empty()) {
    changed();
    return;
  }
  done(0);
}

void FormModal::layout(Plane & screen, int rows, int cols) {
  int label_width = 0;
  for (const FormField & f : fields_) label_width = std::max(label_width, utf8::displayWidth(f.label));
  const int n = static_cast<int>(fields_.size());
  // Padding, title, a gap, the fields, a gap, the error, the summary, a gap, the buttons, the hint, padding.
  const int height = std::min(rows, n + 10);
  const int width = std::min(std::max(20, cols - 2), std::min(kMaxWidth, std::max(kMinWidth, label_width + kFieldWidth + 6)));
  const int field_width = std::max(8, width - label_width - 6);

  if (!plane_) {
    plane_ = screen.createChild();
    plane_->setMouseCursor(MouseCursor::DEFAULT);
    plane_->setElevation(screen.getStyles().theme.elevation.modal);
    plane_->setCornerRadius(screen.getStyles().theme.dialog_radius_px);
  }
  plane_->resize(height, width);
  const StyleProvider & styles = plane_->getStyles();
  const Color bg = styles.picker_bg_color, fg = styles.window_fg_color, strong = styles.window_accent_fg_color, quiet = styles.theme.text_secondary;
  const Color wrong(0xf8, 0x51, 0x49);
  auto paint = [&](const Color & foreground, const Color & background) {
    plane_->setFgColor(foreground);
    plane_->setBgColor(background);
  };
  paint(fg, bg);
  for (int y = 0; y < height; y++) plane_->putstr(y, 0, std::string(static_cast<size_t>(width), ' '));
  const int inner = width - 2;
  const TextStyle plain{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kPlainFace, TextAlign::LEFT};

  paint(strong, bg);
  plane_->putText(1, 1, inner, title_, TextStyle{TextFont::PROPORTIONAL, TextOverflow::ELLIPSIS, kBoldFace, TextAlign::LEFT});

  for (int i = 0; i < n; i++) {
    const int y = 3 + i, x = 1 + label_width + 2;
    const bool here = focus_ == static_cast<size_t>(i);
    const bool bad = !fieldError(static_cast<size_t>(i)).empty() && (attempted_ || !trimmed(text(static_cast<size_t>(i))).empty());
    paint(here ? strong : quiet, bg);
    plane_->putText(y, 1, label_width, fields_[static_cast<size_t>(i)].label, plain);
    field_spots_[static_cast<size_t>(i)] = {y, x, field_width};
    paint(bad ? wrong : strong, here ? styles.theme.surface_raised : styles.window_bg_color);
    plane_->putstr(y, x, std::string(static_cast<size_t>(field_width), ' '));
    const LineEditor::View view = const_cast<LineEditor &>(editors_[static_cast<size_t>(i)]).view(field_width - 1);
    if (view.text.empty() && !here && !fields_[static_cast<size_t>(i)].hint.empty()) {
      paint(quiet, styles.window_bg_color);
      plane_->putstr(y, x, utf8::truncateToWidth(fields_[static_cast<size_t>(i)].hint, field_width));
    } else {
      plane_->putstr(y, x, view.text);
      if (here) {
        // The cell the cursor is on, in the cursor's colours, with the character that is there.
        std::string under = " ";
        int at = 0;
        for (const std::string & cluster : utf8::clusters(view.text)) {
          if (at == view.cursor_column) {
            under = cluster;
            break;
          }
          at += utf8::displayWidth(cluster);
        }
        paint(styles.highlight_fg_color, styles.highlight_bg_color);
        plane_->putstr(y, x + view.cursor_column, under);
      }
    }
  }

  const int error_row = 3 + n + 1, summary_row = error_row + 1, button_row = summary_row + 2, hint_row = button_row + 1;
  paint(wrong, bg);
  plane_->putText(error_row, 1, inner, error_, plain);
  paint(strong, bg);
  plane_->putText(summary_row, 1, inner, summary_, plain);

  // OK and Cancel at the right, side by side; the focused one has the highlight, the pressed one the pressed look.
  const int ok_w = utf8::displayWidth(kOk), cancel_w = utf8::displayWidth(kCancel);
  const int cancel_x = width - 1 - cancel_w, ok_x = cancel_x - 2 - ok_w;
  auto button = [&](int x, const char * label, size_t which, int pressed_index) {
    const bool focused = focus_ == which;
    const bool pressed = pressed_ == pressed_index && over_;
    paint(focused ? styles.highlight_fg_color : strong, pressed ? styles.button_pressed_bg_color : focused ? styles.highlight_bg_color : styles.button_bg_color);
    plane_->putstr(button_row, x, label);
  };
  button(ok_x, kOk, fields_.size(), static_cast<int>(fields_.size()));
  button(cancel_x, kCancel, fields_.size() + 1, static_cast<int>(fields_.size()) + 1);
  ok_ = {button_row, ok_x, ok_w};
  cancel_ = {button_row, cancel_x, cancel_w};
  paint(quiet, bg);
  plane_->putText(hint_row, 1, inner, kHint, plain);

  top_ = (rows - height) / 2;
  left_ = (cols - width) / 2;
  plane_->move(top_, left_);
  plane_->moveToTop();
}

int FormModal::targetAt(int screen_row, int screen_col) const {
  const int row = screen_row - top_, col = screen_col - left_;
  for (size_t i = 0; i < field_spots_.size(); i++) {
    const Spot & s = field_spots_[i];
    if (row == s.row && col >= s.col && col < s.col + s.width) return static_cast<int>(i);
  }
  const int n = static_cast<int>(fields_.size());
  if (row == ok_.row && col >= ok_.col && col < ok_.col + ok_.width) return n;
  if (row == cancel_.row && col >= cancel_.col && col < cancel_.col + cancel_.width) return n + 1;
  return -1;
}

void FormModal::keyDown(const KeyDown & e) {
  const size_t n = fields_.size();
  const bool plain = !e.mods.ctrl && !e.mods.alt && !e.mods.meta;
  if (e.key == Key::Escape || (e.mods.ctrl && e.key == 'g')) {
    done(kClosed);
    return;
  }
  if (e.key == Key::Tab || (plain && (e.key == Key::Down || e.key == Key::Up))) {
    const bool back = (e.key == Key::Tab && e.mods.shift) || e.key == Key::Up;
    focus_ = (focus_ + (back ? n + 1 : 1)) % (n + 2);
    changed();
    return;
  }
  if (e.key == Key::Enter) {
    if (focus_ == n + 1) done(kClosed);
    else accept();
    return;
  }
  if (focus_ >= n) {  // on a button
    if (e.key == ' ' && plain) {
      if (focus_ == n) accept();
      else done(kClosed);
    }
    return;
  }
  LineEditor & editor = editors_[focus_];
  if (editor.key(e)) {
    changed();
  } else if (!e.mods.ctrl && !e.mods.alt && !e.mods.meta && !e.text.empty()) {
    editor.insert(e.text);
    changed();
  }
}

void FormModal::paste(const Paste & e) {
  if (focus_ >= fields_.size()) return;
  editors_[focus_].insert(e.text);
  changed();
}

void FormModal::mouseDown(const MouseDown & e) {
  if (e.button != 1) return;
  const int target = targetAt(e.row, e.col);
  const int n = static_cast<int>(fields_.size());
  if (target >= 0 && target < n) {
    focus_ = static_cast<size_t>(target);
    pressed_ = -1;
    changed();
  } else if (target >= n) {
    pressed_ = target;
    over_ = true;
    relayout();
  }
}

void FormModal::mouseMove(const MouseMove & e) {
  if (pressed_ < 0 || !(e.buttons & 1u)) return;
  // The pressed look follows the pointer: off the button, nothing is pressed.
  const bool over = targetAt(e.row, e.col) == pressed_;
  if (over == over_) return;
  over_ = over;
  relayout();
}

void FormModal::mouseUp(const MouseUp & e) {
  if (e.button != 1 || pressed_ < 0) return;
  const int pressed = pressed_;
  pressed_ = -1;
  if (targetAt(e.row, e.col) == pressed) {
    if (pressed == static_cast<int>(fields_.size())) {
      accept();
      return;
    }
    done(kClosed);
    return;
  }
  relayout();
}

}  // namespace cellui
