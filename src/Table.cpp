#include <cellui/Table.h>

#include <cellui/NumberParse.h>
#include <cellui/Scrollbar.h>
#include <cellui/Utf8.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace cellui {

namespace {

constexpr int kGap = 1;          // cells between columns
constexpr int kMinFlexWidth = 4;
constexpr int kWheelStep = 1;    // rows per isolated wheel notch, more in a quick spin

bool asNumber(const std::string & text, double & out) {
  std::string s = text;
  while (!s.empty() && (s.back() == '%' || s.back() == ' ')) s.pop_back();
  size_t start = 0;
  while (start < s.size() && s[start] == ' ') start++;
  if (start < s.size() && s[start] == '+') start++;
  if (start >= s.size()) return false;
  auto result = parseFloat(s.data() + start, s.data() + s.size(), out);
  return result.ec == std::errc() && result.ptr == s.data() + s.size();
}

std::string lower(std::string s) {
  for (char & c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

TextAlign textAlign(Align align) {
  switch (align) {
    case Align::RIGHT: return TextAlign::RIGHT;
    case Align::CENTER: return TextAlign::CENTER;
    default: return TextAlign::LEFT;
  }
}

}  // namespace

std::optional<double> TableModel::number(int row, int column) const {
  double value = 0;
  if (asNumber(text(row, column), value)) return value;
  return std::nullopt;
}

int TableModel::compare(int a, int b, int column) const {
  std::optional<double> na = number(a, column), nb = number(b, column);
  if (na && nb) return *na < *nb ? -1 : *na > *nb ? 1 : 0;
  return lower(text(a, column)).compare(lower(text(b, column)));
}

Table::Table(Plane & parent, const Charts & charts) : Widget(parent), charts_(charts) {
  setFocusable(true);
  connections_.push_back(on<MouseDown>([this](const MouseDown & e, EventContext &) {
    if (e.button != 1) return Handled::no;
    if (beginThumbDrag(e.row, e.col)) return Handled::yes;
    int heading = -1, position = -1;
    hitAt(e.row, e.col, heading, position);
    pressing_ = true;
    press_on_heading_ = heading >= 0;
    press_button_ = position >= 0 ? buttonAt(e.col - getPosition().second) : std::pair<int, int>{-1, -1};
    setPressed(heading, position);
    return Handled::yes;
  }));
  // The pointer is grabbed from the press to the release, so a drag is still ours: the pressed look moves with it.
  connections_.push_back(on<MouseMove>([this](const MouseMove & e, EventContext &) {
    if (thumb_drag_.active()) {
      if (e.buttons & 1) dragThumb(e.row);
      return Handled::yes;
    }
    if (!pressing_ || !(e.buttons & 1)) return Handled::no;
    if (press_on_heading_) {
      int heading = -1, position = -1;
      hitAt(e.row, e.col, heading, position);
      setPressed(heading, -1);  // among the headings; over none, nothing is pressed
    } else {
      // Among the rows, whatever the column; past the first or last the view scrolls (a move at a time).
      setPressed(-1, dragPosition(e.row, true));
    }
    return Handled::yes;
  }));
  connections_.push_back(on<MouseUp>([this](const MouseUp & e, EventContext &) {
    if (e.button == 1 && thumb_drag_.active()) {
      dragThumb(e.row);
      thumb_drag_.end();
      return Handled::yes;
    }
    if (e.button != 1 || !pressing_) return Handled::no;
    int heading = -1, position = -1;
    hitAt(e.row, e.col, heading, position);
    if (!press_on_heading_) position = dragPosition(e.row, false);
    const int pressed = pressed_position_;
    pressing_ = false;
    setPressed(-1, -1);
    if (press_on_heading_) {
      if (heading >= 0) toggleSort(heading);
    } else if (press_button_.first >= 0) {
      // A press on a button acts only if it is released on the same button of the same row.
      int under = -1, over = -1;
      hitAt(e.row, e.col, under, over);
      if (over >= 0 && over == pressed && buttonAt(e.col - getPosition().second) == press_button_) {
        button_clicked.emit(order_[static_cast<size_t>(over)], press_button_.first, press_button_.second);
      }
    } else if (position >= 0) {
      selectPosition(position);
    }
    return Handled::yes;
  }));
  connections_.push_back(on<Wheel>([this](const Wheel & e, EventContext &) {
    if (e.dy == 0) return Handled::no;
    rows_.setExtent(static_cast<int>(order_.size()), visibleRows());
    rows_.scrollBy(-e.dy * wheel_pace_.step(kWheelStep));
    render();
    return Handled::yes;
  }));
  connections_.push_back(on<KeyDown>([this](const KeyDown & e, EventContext &) { return key(e) ? Handled::yes : Handled::no; }));
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

void Table::setColumns(std::vector<TableColumn> columns) {
  columns_ = std::move(columns);
  for (TableColumn & column : columns_)
    if (!column.buttons.empty()) column.sortable = false;
  if (sort_column_ >= static_cast<int>(columns_.size())) {
    sort_column_ = -1;
    sort_order_ = SortOrder::NONE;
  }
  resort();
  render();
}

void Table::setModel(std::shared_ptr<TableModel> model) {
  model_ = std::move(model);
  selected_ = -1;
  rows_ = ScrollAxis();
  resort();
  render();
}

void Table::modelChanged() {
  if (model_ && selected_ >= model_->rowCount()) selected_ = -1;
  resort();
  render();
}

void Table::setFont(TextFont font) {
  font_ = font;
  render();
}

void Table::setRowHeight(int cells) {
  row_height_ = std::max(1, cells);
  render();
}

void Table::onResize() { render(); }

int Table::visibleRows() const {
  int rows = getDim().first - 1;  // the heading
  return std::max(0, rows / row_height_);
}

void Table::resort() {
  int n = model_ ? model_->rowCount() : 0;
  order_.resize(static_cast<size_t>(n));
  for (int i = 0; i < n; i++) order_[static_cast<size_t>(i)] = i;
  if (model_ && sort_column_ >= 0 && sort_order_ != SortOrder::NONE) {
    const int column = sort_column_;
    const bool down = sort_order_ == SortOrder::DESCENDING;
    std::stable_sort(order_.begin(), order_.end(), [&](int a, int b) { return down ? model_->compare(b, a, column) < 0 : model_->compare(a, b, column) < 0; });
  }
  rows_.setExtent(n, visibleRows());
}

void Table::sortBy(int column, SortOrder order) {
  if (column < 0 || column >= static_cast<int>(columns_.size()) || order == SortOrder::NONE) {
    column = -1;
    order = SortOrder::NONE;
  }
  sort_column_ = column;
  sort_order_ = order;
  resort();
  if (selected_ >= 0) rows_.reveal(selectedPosition());
  render();
  sort_changed.emit(sort_column_, sort_order_);
}

void Table::toggleSort(int column) {
  if (column < 0 || column >= static_cast<int>(columns_.size()) || !columns_[static_cast<size_t>(column)].sortable) return;
  if (column != sort_column_) sortBy(column, SortOrder::ASCENDING);
  else if (sort_order_ == SortOrder::ASCENDING) sortBy(column, SortOrder::DESCENDING);
  else sortBy(-1, SortOrder::NONE);
}

int Table::selectedPosition() const {
  if (selected_ < 0) return -1;
  auto it = std::find(order_.begin(), order_.end(), selected_);
  return it == order_.end() ? -1 : static_cast<int>(it - order_.begin());
}

void Table::select(int model_row) {
  if (!model_ || model_row < 0 || model_row >= model_->rowCount()) model_row = -1;
  if (model_row == selected_) return;
  selected_ = model_row;
  rows_.setExtent(static_cast<int>(order_.size()), visibleRows());
  if (selected_ >= 0) rows_.reveal(selectedPosition());
  render();
  selection_changed.emit(selected_);
}

void Table::selectPosition(int position) {
  if (order_.empty()) return;
  select(order_[static_cast<size_t>(std::clamp(position, 0, static_cast<int>(order_.size()) - 1))]);
}

void Table::moveSelection(int delta) {
  int at = selectedPosition();
  selectPosition(at < 0 ? (delta > 0 ? 0 : static_cast<int>(order_.size()) - 1) : at + delta);
}

void Table::hitAt(int screen_row, int screen_col, int & heading, int & position) const {
  heading = position = -1;
  auto [py, px] = getPosition();
  int y = screen_row - py, x = screen_col - px;
  if (y < 0 || y >= getDim().first || x < 0 || x >= getDim().second) return;
  if (y == 0) {
    int c = columnAt(x);
    if (c >= 0 && columns_[static_cast<size_t>(c)].sortable) heading = c;
    return;
  }
  // The row under the middle of the cell, as the view moves.
  int at = rows_.indexAt((y - 1) / row_height_);
  if ((y - 1) / row_height_ < visibleRows() && at >= 0 && at < static_cast<int>(order_.size())) position = at;
}

// A press on the scrollbar's column below the heading takes the thumb (or jumps it to the pointer).
bool Table::beginThumbDrag(int screen_row, int screen_col) {
  auto [py, px] = getPosition();
  int y = screen_row - py, x = screen_col - px;
  const int track = std::max(0, getDim().first - 1), count = static_cast<int>(order_.size());
  if (x != getDim().second - 1 || y < 1 || y > track) return false;
  rows_.setExtent(count, visibleRows());
  auto jump = thumb_drag_.begin(track, count, visibleRows(), rows_.shown(), y - 1);
  if (!thumb_drag_.active()) return false;
  if (jump) {
    rows_.jumpTo(*jump);
    render();
  }
  return true;
}

void Table::dragThumb(int screen_row) {
  const int track = std::max(0, getDim().first - 1), count = static_cast<int>(order_.size());
  rows_.setExtent(count, visibleRows());
  rows_.jumpTo(thumb_drag_.offset(track, count, visibleRows(), screen_row - getPosition().first - 1));
  render();
}

int Table::dragPosition(int screen_row, bool scroll) {
  const int count = static_cast<int>(order_.size());
  if (count == 0) return -1;
  rows_.setExtent(count, visibleRows());
  const int y = screen_row - getPosition().first;
  if (y < 1 || (y - 1) / row_height_ < 0) {
    if (scroll && rows_.offset() > 0) {
      rows_.scrollBy(-1);
      render();
    }
    return rows_.offset();
  }
  if ((y - 1) / row_height_ >= visibleRows()) {
    if (scroll && !rows_.atEnd()) {
      rows_.scrollBy(1);
      render();
    }
    return std::min(count - 1, rows_.offset() + visibleRows() - 1);
  }
  int at = rows_.indexAt((y - 1) / row_height_);
  return std::clamp(at, 0, count - 1);
}

void Table::setPressed(int heading, int position) {
  if (heading == pressed_heading_ && position == pressed_position_) return;
  pressed_heading_ = heading;
  pressed_position_ = position;
  render();
}

int Table::columnWidth(size_t c) const {
  const TableColumn & column = columns_[c];
  if (column.buttons.empty()) return column.width;
  int w = static_cast<int>(column.buttons.size()) - 1;  // one cell between buttons
  for (const ButtonFace & face : column.buttons) w += face.width();
  return w;
}

std::pair<int, int> Table::buttonAt(int local_col) const {
  const int c = columnAt(local_col);
  if (c < 0 || columns_[static_cast<size_t>(c)].buttons.empty()) return {-1, -1};
  int x = layout().x[static_cast<size_t>(c)];
  for (size_t i = 0; i < columns_[static_cast<size_t>(c)].buttons.size(); i++) {
    const int w = columns_[static_cast<size_t>(c)].buttons[i].width();
    if (local_col >= x && local_col < x + w) return {c, static_cast<int>(i)};
    x += w + 1;
  }
  return {-1, -1};
}

int Table::columnAt(int local_col) const {
  Layout l = layout();
  int found = -1;
  for (size_t i = 0; i < l.x.size(); i++) {
    if (l.width[i] == 0 && columns_[i].icon) continue;  // not shown
    if (local_col >= l.x[i]) found = static_cast<int>(i);
  }
  if (found >= 0 && local_col >= l.x[static_cast<size_t>(found)] + l.width[static_cast<size_t>(found)] + kGap) return -1;
  return found;
}

bool Table::key(const KeyDown & e) {
  if (e.mods.alt || e.mods.meta) return false;
  const int page = std::max(1, visibleRows() - 1);
  if (e.mods.ctrl) {
    if (e.key == 'n') moveSelection(1);
    else if (e.key == 'p') moveSelection(-1);
    else return false;
    return true;
  }
  if (e.key == Key::Down) moveSelection(1);
  else if (e.key == Key::Up) moveSelection(-1);
  else if (e.key == Key::PageDown) moveSelection(page);
  else if (e.key == Key::PageUp) moveSelection(-page);
  else if (e.key == Key::Home) selectPosition(0);
  else if (e.key == Key::End) selectPosition(static_cast<int>(order_.size()) - 1);
  else if (e.key == Key::Enter) {
    if (selected_ >= 0) activated.emit(selected_);
  } else {
    return false;
  }
  return true;
}

Table::Layout Table::layout() const {
  Layout l;
  const int cols = getDim().second;
  const int usable = std::max(0, cols - 1);  // the last column is the scrollbar
  int fixed = 0, flex = 0, count = 0;
  for (size_t i = 0; i < columns_.size(); i++) {
    if (!shown(i)) continue;
    count++;
    if (columnWidth(i) > 0) fixed += columnWidth(i);
    else flex++;
  }
  int gaps = count == 0 ? 0 : kGap * (count - 1);
  int left = std::max(0, usable - fixed - gaps);
  int share = flex ? std::max(kMinFlexWidth, left / flex) : 0;
  int x = 0;
  for (size_t i = 0; i < columns_.size(); i++) {
    if (!shown(i)) {
      l.x.push_back(x);
      l.width.push_back(0);
      continue;
    }
    int w = columnWidth(i) > 0 ? columnWidth(i) : share;
    l.x.push_back(x);
    l.width.push_back(w);
    x += w + kGap;
  }
  l.scrollbar_x = cols - 1;
  return l;
}

bool Table::shown(size_t c) const {
  if (!columns_[c].icon) return true;
  int h = 0, w = 0;
  return getPlane().cellPixelSize(h, w) && h > 0 && w > 0;
}

bool Table::animating() const { return rows_.animating(); }

bool Table::tick(double seconds) {
  if (!rows_.animating()) return false;
  rows_.tick(seconds);
  render();
  return rows_.animating();
}

void Table::render() {
  if (!hasPlane()) return;
  auto [rows, cols] = getDim();
  if (rows <= 0 || cols <= 0) return;
  const StyleProvider & styles = getPlane().getStyles();
  const int count = static_cast<int>(order_.size()), view = visibleRows();
  const bool smooth = getPlane().smoothScroll() && row_height_ == 1;
  rows_.setSmooth(smooth);
  rows_.setExtent(count, view);
  getPlane().erase();
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
  fill();
  Layout l = layout();
  drawHeading(l, styles);
  drawScrollbar(l, styles);

  // The rows are on a plane of their own under the heading, beside the scrollbar's column. The tree being flat or
  // not decides whether where it goes is in the table's columns or the root's.
  if (!body_) body_ = getPlane().createChild();
  auto [origin_row, origin_col] = getPlane().childOrigin();
  body_->move(origin_row + 1, origin_col);
  body_->resize(view * row_height_, std::max(0, cols - 1));
  body_->setScrollOffset(smooth ? rows_.fraction() : 0.0, 0.0);
  body_->erase();
  swapPlane(body_);  // the drawing helpers now draw on the rows' plane; body_ holds the table's own
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
  const int first = rows_.firstShown(), drawn = view + (smooth ? 1 : 0);
  for (int i = 0; i < drawn && first + i < count; i++) drawRow(l, styles, first + i, i * row_height_);
  swapPlane(body_);
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
}

void Table::drawHeading(const Layout & l, const StyleProvider & styles) {
  const int cols = getDim().second;
  setFgColor(styles.window_accent_fg_color);
  setBgColor(styles.heading_bg_color);
  putstr(0, 0, std::string(static_cast<size_t>(cols), ' '));
  for (size_t i = 0; i < columns_.size(); i++) {
    if (l.width[i] == 0) continue;
    std::string title = columns_[i].title;
    if (static_cast<int>(i) == sort_column_) title += sort_order_ == SortOrder::DESCENDING ? " ▼" : " ▲";
    // A heading that does not fit loses its end, but keeps the arrow: the sorted column must say so.
    if (utf8::displayWidth(title) > l.width[i] && static_cast<int>(i) == sort_column_) {
      std::string arrow = sort_order_ == SortOrder::DESCENDING ? " ▼" : " ▲";
      title = Plane::cutToCells(columns_[i].title, std::max(0, l.width[i] - 2), TextOverflow::CLIP) + arrow;
    }
    const bool pressed = static_cast<int>(i) == pressed_heading_;
    if (pressed) setBgColor(styles.button_pressed_bg_color);
    putText(0, l.x[i], l.width[i], title, TextStyle{font_, TextOverflow::ELLIPSIS, kBoldFace, textAlign(columns_[i].align)});
    if (pressed) setBgColor(styles.heading_bg_color);
  }
}

// Icons are made for the window's background: light ink on a dark window, dark on a light one.
static bool lightIcons(const StyleProvider & styles) {
  const std::array<uint8_t, 3> w = styles.window_bg_color.toSrgb8();
  return 0.2126f * static_cast<float>(w[0]) + 0.7152f * static_cast<float>(w[1]) + 0.0722f * static_cast<float>(w[2]) < 127.5f;
}

void Table::drawRow(const Layout & l, const StyleProvider & styles, int position, int y) {
  const int cols = getDim().second;
  const int model_row = order_[static_cast<size_t>(position)];
  const bool selected = model_row == selected_;
  const bool pressed = position == pressed_position_;
  Color bg = pressed ? styles.row_pressed_bg_color
                     : selected ? (focused_ ? styles.highlight_bg_color : styles.highlight_unfocused_bg_color) : styles.window_bg_color;
  Color fg = pressed ? styles.window_accent_fg_color : selected && focused_ ? styles.highlight_fg_color : styles.window_fg_color;
  setBgColor(bg);
  setFgColor(fg);
  for (int r = 0; r < row_height_; r++) putstr(y + r, 0, std::string(static_cast<size_t>(cols), ' '));
  for (size_t c = 0; c < columns_.size(); c++) {
    const int col = static_cast<int>(c);
    if (l.width[c] == 0) continue;
    if (columns_[c].icon) {
      drawIcon(y, l.x[c], l.width[c], model_->icon(model_row, col), selected && focused_, styles.highlight_bg_color, styles.highlight_fg_color, lightIcons(styles));
      continue;
    }
    if (!columns_[c].buttons.empty()) {
      setFgColor((selected && focused_) || pressed ? fg : styles.window_accent_fg_color);
      int x = l.x[c];
      for (const ButtonFace & face : columns_[c].buttons) {
        putstr(y + row_height_ / 2, x, face.glyph);
        x += face.width() + 1;
      }
      continue;
    }
    std::optional<Color> color = model_->color(model_row, col);
    if (columns_[c].trace) {
      TraceLook look{bg, color.value_or(fg), color ? color : std::nullopt};
      uint64_t id = (static_cast<uint64_t>(model_row) + 1) * 64 + static_cast<uint64_t>(c) + 1;
      charts_.drawTrace(getPlane(), y, l.x[c], row_height_, l.width[c], model_->trace(model_row, col), look, 0, getDim().first + 1, id);
      setBgColor(bg);
    } else {
      setFgColor((selected && focused_) || pressed ? fg : color.value_or(fg));
      putText(y + row_height_ / 2, l.x[c], l.width[c], model_->text(model_row, col), TextStyle{font_, TextOverflow::ELLIPSIS, kPlainFace, textAlign(columns_[c].align)});
    }
  }
}

// The display fits the picture into the cell's box, as large as it goes with its proportions kept. On the focused
// selection the picture is desaturated and recoloured as a duotone between the highlight's background and its
// foreground (`paper`, `ink`), alpha kept, so a logo keeps its detail on the highlight whatever the highlight's
// colours are (tinting by the foreground alone makes every pixel black where the foreground is). The picture's own
// ink is taken to be what stands out from the window's background, so it becomes the highlight's foreground: the
// inversion, where the highlight's polarity differs from the window's. It has a key of its own, so the plain picture
// stays shared.
void Table::drawIcon(int y, int x, int width, const std::shared_ptr<const TableIcon> & icon, bool inverted, const Color & paper, const Color & ink,
                     bool light_ink) {
  if (!icon || !icon->pixels || icon->width <= 0 || icon->height <= 0) return;
  const std::array<uint8_t, 3> pa = paper.toSrgb8(), in = ink.toSrgb8();
  auto render = [icon, inverted, pa, in, light_ink] {
    std::vector<uint32_t> pixels = *icon->pixels;
    if (inverted) {
      for (uint32_t & p : pixels) {
        auto * b = reinterpret_cast<unsigned char *>(&p);  // bytes R, G, B, A in memory
        const float gray = (0.2126f * static_cast<float>(b[0]) + 0.7152f * static_cast<float>(b[1]) + 0.0722f * static_cast<float>(b[2])) / 255.0f;
        const float t = light_ink ? gray : 1.0f - gray;  // how much of the picture's ink is here
        for (size_t k = 0; k < 3; k++) b[k] = static_cast<unsigned char>(std::lround(static_cast<float>(pa[k]) + t * (static_cast<float>(in[k]) - static_cast<float>(pa[k]))));
      }
    }
    return pixels;
  };
  getPlane().drawSharedImageFitted(y, x, row_height_, width, ImageKey{inverted ? "icon-inverted" : "icon", icon->id}, icon->width, icon->height, render, 0,
                                   getDim().first + 1);
}

void Table::drawScrollbar(const Layout & l, const StyleProvider & styles) {
  cellui::drawScrollbar(getPlane(), 1, l.scrollbar_x, std::max(0, getDim().first - 1), static_cast<int>(order_.size()), visibleRows(), rows_.shown(),
                        styles.window_bg_color, styles.theme.surface_overlay, styles.window_accent_fg_color);
}

}  // namespace cellui
