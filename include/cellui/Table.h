#pragma once

#include <cellui/Button.h>
#include <cellui/Charts.h>
#include <cellui/InputEvents.h>
#include <cellui/Scrollbar.h>
#include <cellui/Signal.h>
#include <cellui/WheelAcceleration.h>
#include <cellui/Viewport.h>
#include <cellui/Widget.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cellui {

enum class Align { LEFT, RIGHT, CENTER };

enum class SortOrder { NONE, ASCENDING, DESCENDING };

// One column of a Table.
struct TableColumn {
  std::string title;
  // Cells wide; 0 takes a share of what the fixed columns leave over.
  int width = 10;
  Align align = Align::LEFT;
  // A click on the heading sorts by the column.
  bool sortable = true;
  // The cells are traces (TableModel::trace()) drawn by the display's Charts,
  // not text.
  bool trace = false;
  // The cells are pictures (TableModel::icon()), shown where the display shows
  // pictures. Where it does not, the column takes no room and no gap.
  bool icon = false;
  // The cells are a row of small buttons, one for each glyph (the same in every row): "[i]", an icon, a word. The
  // column is as wide as its buttons (one cell between them), not `width`, and is never sorted. A click on one emits
  // Table::button_clicked with its row and the button's index, and leaves the selection where it is.
  std::vector<ButtonFace> buttons = {};
};

// A small picture in an icon column's cell: width*height pixels, row 0 at the
// top, bytes R, G, B, A. `id` names the picture: every row that shows the same
// id shows the same pixels, which the display then keeps once.
struct TableIcon {
  uint64_t id = 0;
  int width = 0, height = 0;
  std::shared_ptr<const std::vector<uint32_t>> pixels;
};

// What a Table shows. Rows are numbered 0 up to rowCount() in the model's
// own order, which is the order the table shows until it is sorted; the
// table never copies the data, it asks for the cells that are in view.
class TableModel {
 public:
  virtual ~TableModel() = default;
  virtual int rowCount() const = 0;
  // The text of a cell, for a column that is not a trace.
  virtual std::string text(int row, int column) const = 0;
  // The values of a trace cell, oldest first (see TraceLook): a view of the
  // model's own values, read while the cell is drawn, so nothing is copied.
  virtual std::span<const float> trace(int /*row*/, int /*column*/) const { return {}; }
  // The picture of a cell in an icon column; null for none (not there yet, or the row has none). Read while the cell
  // is drawn, so it must not wait for anything.
  virtual std::shared_ptr<const TableIcon> icon(int /*row*/, int /*column*/) const { return nullptr; }
  // The cell's colour: the text's, or a trace's line. Unset is the theme's.
  virtual std::optional<Color> color(int /*row*/, int /*column*/) const { return std::nullopt; }
  // The cell's value, which it sorts by: a model with numbers gives them
  // here, so a sort never reads them back from rounded text. The default
  // reads the text as a number when it is one (a sign and a trailing % are
  // allowed); unset for text.
  virtual std::optional<double> number(int row, int column) const;
  // Negative if row `a` sorts before row `b` by `column`, positive if after,
  // 0 if neither: by number() when both have one, else by text, letters of
  // either case alike. A trace column sorts as equal.
  virtual int compare(int a, int b, int column) const;
};

// A scrollable table with a heading row, the rows of a TableModel and a
// selected row. A click on a heading sorts by that column (ascending, then
// descending, then the model's order again); a click on a row selects it; both
// act when the button is released over what it was pressed on, and show a
// pressed look while it is held (moving off cancels); the wheel scrolls and the arrow, page, Home and End keys move the selection.
// A trace column shows each row's series as a small line chart in the
// display's own version (pictures where it has them, braille dots where it
// has not).
//
// It lays out with the plane's own position in the parent, so a table sits on
// the screen's root plane, as a SpinBox does. Redraws itself whenever
// something changed.
class Table : public Widget {
 public:
  Table(Plane & parent, const Charts & charts);
  ~Table() override = default;

  void setColumns(std::vector<TableColumn> columns);
  const std::vector<TableColumn> & columns() const { return columns_; }

  // The rows to show. The selection, the scroll position and the sort are
  // reset, except that a sort by a column that still exists is kept.
  void setModel(std::shared_ptr<TableModel> model);
  // The model's rows or cells changed in place (a new price): sorts again and
  // redraws. The selected row stays selected as long as the model has it.
  void modelChanged();
  const std::shared_ptr<TableModel> & model() const { return model_; }

  // How many cells tall a row is (default 1). Text is on the middle one; a
  // trace has all of them, so a taller row draws a finer chart.
  void setRowHeight(int cells);
  int rowHeight() const { return row_height_; }

  // The face the text of the headings and cells is set in: the display's proportional one by default (a display
  // that lays text out itself draws it, another the cells' own), or the cells' own everywhere.
  void setFont(TextFont font);
  TextFont font() const { return font_; }

  void sortBy(int column, SortOrder order);
  int sortColumn() const { return sort_column_; }
  SortOrder sortOrder() const { return sort_order_; }
  // What a click on the heading does: the next order of the column, or
  // ascending for another column.
  void toggleSort(int column);

  // The selected row as the model numbers it, or -1 for none.
  int selectedRow() const { return selected_; }
  void select(int model_row);
  // Where the selected row is in what is shown, or -1.
  int selectedPosition() const;
  // The model row shown at `position` (0 is the first row after sorting).
  int rowAt(int position) const { return position >= 0 && position < static_cast<int>(order_.size()) ? order_[static_cast<size_t>(position)] : -1; }
  // How many rows fit under the heading.
  int visibleRows() const;

  // Where the display scrolls smoothly (DisplayInfo::smooth_scroll), the rows ease to a new scroll position instead
  // of jumping to it (a row height of 1 only). The easing moves with time: call tick() for each frame, with the
  // seconds since the last (Application::frame_started), and ask for another frame while it says it is still moving.
  bool animating() const;
  // Moves the rows toward where they were scrolled to by `seconds` and redraws them; returns whether they are still moving.
  bool tick(double seconds);
  // The position of the first row in view, with the fraction of a row it is scrolled into the next while it eases.
  double scrollPosition() const { return rows_.shown(); }
  // The position of the first row in view.
  int firstVisiblePosition() const { return rows_.offset(); }

  // The model row selected, activated (Enter), and the new sort.
  Signal<int> selection_changed;
  Signal<int> activated;
  Signal<int, SortOrder> sort_changed;
  // A click on a button of a buttons column (TableColumn::buttons): the model row, the column and the button's index.
  // The selection does not move: the click is about that row, not a choice of it.
  Signal<int, int, int> button_clicked;

  // Draws the table; every change above does it already.
  void render();

 protected:
  void onResize() override;

 private:
  struct Layout {
    std::vector<int> x, width;
    int scrollbar_x = 0;
  };
  // Whether column `c` is drawn: an icon column only where the display shows pictures.
  bool shown(size_t c) const;
  Layout layout() const;
  void resort();
  void moveSelection(int delta);
  void selectPosition(int position);
  // What is under a screen cell: the heading's column (sortable ones only) or the position of a row, -1 for none.
  void hitAt(int screen_row, int screen_col, int & heading, int & position) const;
  void setPressed(int heading, int position);
  // Dragging the scrollbar's thumb (the pointer is grabbed from the press to the release).
  bool beginThumbDrag(int screen_row, int screen_col);
  void dragThumb(int screen_row);
  // The row a drag that began on a row is over, by the pointer's screen row alone: above the rows (the heading
  // included) the first one in view, below them the last, and with `scroll` the view moves one row that way, so a
  // drag past an edge reaches the rows beyond it. -1 when there are no rows.
  int dragPosition(int screen_row, bool scroll);
  int columnAt(int local_col) const;
  // The cells column `c` takes: its own width, or its buttons' together.
  int columnWidth(size_t c) const;
  // The button of a buttons column at a table column, as {column, index}; {-1, -1} for none.
  std::pair<int, int> buttonAt(int local_col) const;
  bool key(const KeyDown & e);
  void drawHeading(const Layout & l, const StyleProvider & styles);
  void drawRow(const Layout & l, const StyleProvider & styles, int position, int y);
  void drawIcon(int y, int x, int width, const std::shared_ptr<const TableIcon> & icon, bool inverted);
  void drawScrollbar(const Layout & l, const StyleProvider & styles);

  const Charts & charts_;
  std::vector<TableColumn> columns_;
  std::shared_ptr<TableModel> model_;
  std::vector<int> order_;
  int sort_column_ = -1;
  SortOrder sort_order_ = SortOrder::NONE;
  int selected_ = -1;
  int row_height_ = 1;
  TextFont font_ = TextFont::PROPORTIONAL;
  ScrollAxis rows_;
  WheelAcceleration wheel_pace_;
  // The rows' own plane over the table's, under the heading: the display shifts it by the part of a row the rows
  // are scrolled into while they ease.
  std::unique_ptr<Plane> body_;
  bool focused_ = false;
  // While the left button is held: the heading or the row it is over (-1 for none) is drawn pressed, and moves with the
  // pointer, among the headings if the button went down on one, among the rows if on a row. A release acts on it.
  int pressed_heading_ = -1, pressed_position_ = -1;
  bool press_on_heading_ = false;
  std::pair<int, int> press_button_{-1, -1};  // the button the press went down on
  bool pressing_ = false;
  ScrollbarDrag thumb_drag_;
  std::vector<Connection> connections_;
};

}  // namespace cellui
