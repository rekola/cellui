#include <cellui/Table.h>

#include <cellui/NumberParse.h>
#include <cellui/Utf8.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace cellui {

namespace {

constexpr int kGap = 1;          // cells between columns
constexpr int kMinFlexWidth = 4;
constexpr int kWheelStep = 3;    // rows per wheel notch

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

std::string aligned(const std::string & text, int width, Align align) {
  std::string cut = Plane::cutToCells(text, width, TextOverflow::ELLIPSIS);
  int gap = std::max(0, width - utf8::displayWidth(cut));
  switch (align) {
    case Align::RIGHT: return std::string(static_cast<size_t>(gap), ' ') + cut;
    case Align::CENTER: return std::string(static_cast<size_t>(gap / 2), ' ') + cut + std::string(static_cast<size_t>(gap - gap / 2), ' ');
    default: return cut + std::string(static_cast<size_t>(gap), ' ');
  }
}

}  // namespace

int TableModel::compare(int column, int a, int b) const {
  std::string ta = text(a, column), tb = text(b, column);
  double na = 0, nb = 0;
  if (asNumber(ta, na) && asNumber(tb, nb)) return na < nb ? -1 : na > nb ? 1 : 0;
  return lower(ta).compare(lower(tb));
}

Table::Table(Plane & parent, const Charts & charts) : Widget(parent), charts_(charts) {
  setFocusable(true);
  connections_.push_back(on<MouseDown>([this](const MouseDown & e, EventContext &) {
    if (e.button != 1) return Handled::no;
    auto [py, px] = getPosition();
    (void)px;
    int y = e.row - py;
    if (y == 0) clickHeading(columnAt(e.col - getPosition().second));
    else if (y > 0) clickBody(y - 1);
    return Handled::yes;
  }));
  connections_.push_back(on<Wheel>([this](const Wheel & e, EventContext &) {
    if (e.dy == 0) return Handled::no;
    rows_.setExtent(static_cast<int>(order_.size()), visibleRows());
    rows_.scrollBy(-e.dy * kWheelStep);
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
    std::stable_sort(order_.begin(), order_.end(), [&](int a, int b) { return down ? model_->compare(column, b, a) < 0 : model_->compare(column, a, b) < 0; });
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

void Table::clickHeading(int column) { toggleSort(column); }

void Table::clickBody(int local_row) {
  rows_.setExtent(static_cast<int>(order_.size()), visibleRows());
  int position = rows_.offset() + local_row / row_height_;
  if (position < static_cast<int>(order_.size())) selectPosition(position);
}

int Table::columnAt(int local_col) const {
  Layout l = layout();
  int found = -1;
  for (size_t i = 0; i < l.x.size(); i++) {
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
  } else if (e.key == '<' || e.key == '>') {
    // The previous or next sortable column, ascending.
    int n = static_cast<int>(columns_.size());
    int step = e.key == '>' ? 1 : -1;
    int c = sort_column_;
    for (int i = 0; i < n; i++) {
      c = c < 0 ? (step > 0 ? 0 : n - 1) : ((c + step) % n + n) % n;
      if (columns_[static_cast<size_t>(c)].sortable) {
        sortBy(c, SortOrder::ASCENDING);
        break;
      }
    }
  } else if (e.key == 'r') {
    if (sort_column_ >= 0) sortBy(sort_column_, sort_order_ == SortOrder::ASCENDING ? SortOrder::DESCENDING : SortOrder::ASCENDING);
  } else {
    return false;
  }
  return true;
}

Table::Layout Table::layout() const {
  Layout l;
  const int cols = getDim().second;
  const int usable = std::max(0, cols - 1);  // the last column is the scrollbar
  int fixed = 0, flex = 0;
  for (auto & c : columns_) {
    if (c.width > 0) fixed += c.width;
    else flex++;
  }
  int gaps = columns_.empty() ? 0 : kGap * (static_cast<int>(columns_.size()) - 1);
  int left = std::max(0, usable - fixed - gaps);
  int share = flex ? std::max(kMinFlexWidth, left / flex) : 0;
  int x = 0;
  for (auto & c : columns_) {
    int w = c.width > 0 ? c.width : share;
    l.x.push_back(x);
    l.width.push_back(w);
    x += w + kGap;
  }
  l.scrollbar_x = cols - 1;
  return l;
}

void Table::render() {
  if (!hasPlane()) return;
  auto [rows, cols] = getDim();
  if (rows <= 0 || cols <= 0) return;
  const StyleProvider & styles = getPlane().getStyles();
  rows_.setExtent(static_cast<int>(order_.size()), visibleRows());
  getPlane().erase();
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
  fill();
  Layout l = layout();
  drawHeading(l, styles);
  const int first = rows_.offset();
  for (int i = 0; i < visibleRows() && first + i < static_cast<int>(order_.size()); i++) drawRow(l, styles, first + i, 1 + i * row_height_);
  drawScrollbar(l, styles);
  setFgColor(styles.window_fg_color);
  setBgColor(styles.window_bg_color);
}

void Table::drawHeading(const Layout & l, const StyleProvider & styles) {
  const int cols = getDim().second;
  setFgColor(styles.window_accent_fg_color);
  setBgColor(styles.heading_bg_color);
  putstr(0, 0, std::string(static_cast<size_t>(cols), ' '));
  for (size_t i = 0; i < columns_.size(); i++) {
    std::string title = columns_[i].title;
    if (static_cast<int>(i) == sort_column_) title += sort_order_ == SortOrder::DESCENDING ? " ▼" : " ▲";
    // A heading that does not fit loses its end, but keeps the arrow: the sorted column must say so.
    if (utf8::displayWidth(title) > l.width[i] && static_cast<int>(i) == sort_column_) {
      std::string arrow = sort_order_ == SortOrder::DESCENDING ? " ▼" : " ▲";
      title = Plane::cutToCells(columns_[i].title, std::max(0, l.width[i] - 2), TextOverflow::CLIP) + arrow;
    }
    putstr(0, l.x[i], aligned(title, l.width[i], columns_[i].align), kBoldFace);
  }
}

void Table::drawRow(const Layout & l, const StyleProvider & styles, int position, int y) {
  const int cols = getDim().second;
  const int model_row = order_[static_cast<size_t>(position)];
  const bool selected = model_row == selected_;
  Color bg = selected ? (focused_ ? styles.highlight_bg_color : styles.highlight_unfocused_bg_color) : styles.window_bg_color;
  Color fg = selected && focused_ ? styles.highlight_fg_color : styles.window_fg_color;
  setBgColor(bg);
  setFgColor(fg);
  for (int r = 0; r < row_height_; r++) putstr(y + r, 0, std::string(static_cast<size_t>(cols), ' '));
  for (size_t c = 0; c < columns_.size(); c++) {
    const int col = static_cast<int>(c);
    std::optional<Color> color = model_->color(model_row, col);
    if (columns_[c].trace) {
      TraceLook look{bg, color.value_or(fg), color ? color : std::nullopt};
      uint64_t id = (static_cast<uint64_t>(model_row) + 1) * 64 + static_cast<uint64_t>(c) + 1;
      charts_.drawTrace(getPlane(), y, l.x[c], row_height_, l.width[c], model_->trace(model_row, col), look, 1, getDim().first, id);
      setBgColor(bg);
    } else {
      setFgColor(selected && focused_ ? fg : color.value_or(fg));
      putstr(y + row_height_ / 2, l.x[c], aligned(model_->text(model_row, col), l.width[c], columns_[c].align));
    }
  }
}

void Table::drawScrollbar(const Layout & l, const StyleProvider & styles) {
  const int body = std::max(0, getDim().first - 1);
  const int content = static_cast<int>(order_.size()), view = visibleRows();
  if (body <= 0 || content <= view) return;
  setBgColor(styles.window_bg_color);
  setFgColor(styles.theme.surface_overlay);
  for (int y = 0; y < body; y++) putstr(1 + y, l.scrollbar_x, "│");
  int thumb = std::max(1, body * view / content);
  int top = (body - thumb) * rows_.offset() / std::max(1, content - view);
  setFgColor(styles.window_accent_fg_color);
  for (int y = 0; y < thumb; y++) putstr(1 + top + y, l.scrollbar_x, "┃");
}

}  // namespace cellui
