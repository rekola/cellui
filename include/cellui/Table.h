#pragma once

#include <cellui/Charts.h>
#include <cellui/InputEvents.h>
#include <cellui/Signal.h>
#include <cellui/Viewport.h>
#include <cellui/Widget.h>

#include <memory>
#include <optional>
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
  // The values of a trace cell, oldest first (see TraceLook).
  virtual std::vector<float> trace(int row, int column) const {
    (void)row;
    (void)column;
    return {};
  }
  // The cell's colour: the text's, or a trace's line. Unset is the theme's.
  virtual std::optional<Color> color(int row, int column) const {
    (void)row;
    (void)column;
    return std::nullopt;
  }
  // Negative if row `a` sorts before row `b` by `column`, positive if after,
  // 0 if neither. The default reads both texts as numbers when they are one
  // (a trailing % is ignored) and compares them as text, letters of either
  // case alike, when they are not; a trace column sorts as equal.
  virtual int compare(int column, int a, int b) const;
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
  // The position of the first row in view.
  int firstVisiblePosition() const { return rows_.offset(); }

  // The model row selected, activated (Enter), and the new sort.
  Signal<int> selection_changed;
  Signal<int> activated;
  Signal<int, SortOrder> sort_changed;

  // Draws the table; every change above does it already.
  void render();

 protected:
  void onResize() override;

 private:
  struct Layout {
    std::vector<int> x, width;
    int scrollbar_x = 0;
  };
  Layout layout() const;
  void resort();
  void moveSelection(int delta);
  void selectPosition(int position);
  // What is under a screen cell: the heading's column (sortable ones only) or the position of a row, -1 for none.
  void hitAt(int screen_row, int screen_col, int & heading, int & position) const;
  void setPressed(int heading, int position);
  int columnAt(int local_col) const;
  bool key(const KeyDown & e);
  void drawHeading(const Layout & l, const StyleProvider & styles);
  void drawRow(const Layout & l, const StyleProvider & styles, int position, int y);
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
  bool focused_ = false;
  // While the left button is held: the heading or the row it is over (-1 for none) is drawn pressed, and moves with the
  // pointer, among the headings if the button went down on one, among the rows if on a row. A release acts on it.
  int pressed_heading_ = -1, pressed_position_ = -1;
  bool press_on_heading_ = false;
  bool pressing_ = false;
  std::vector<Connection> connections_;
};

}  // namespace cellui
