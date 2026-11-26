#pragma once

#include <cellui/ModalDialog.h>

#include <memory>
#include <string>
#include <vector>

namespace cellui {

// A context menu: a plain list of items that opens in place, under (or, where there is no room, over) the cell it is opened from,
// on a raised panel with a shadow and a border, with no dimming and no blur of what lies behind. It is modal only in that it takes
// the input while it is up, as a menu of an operating system does: Up and Down (and Ctrl+P, Ctrl+N) move the highlight, past
// separators and disabled items and round the ends, Home and End go to the first and last, Enter or Space chooses, Escape (or q,
// Ctrl+G) closes; the pointer highlights the item under it and a click on an item chooses it; a click anywhere else closes it
// (and goes no further). showModal() returns the index of the item chosen, or kClosed.
class PopupMenu : public ModalDialog {
 public:
  struct Item {
    std::string label;  // empty with `separator`
    bool separator = false;
    bool enabled = true;
  };

  PopupMenu(Widget & root, Plane & screen, EventLoop & loop, std::vector<Item> items);
  // Plain items, all enabled.
  PopupMenu(Widget & root, Plane & screen, EventLoop & loop, const std::vector<std::string> & labels);

  // The screen cell it opens from (the button that was pressed): the menu's right edge lines up with that column where there is
  // room, and it goes below the row, or above where it does not fit below. Without one it opens at the top left.
  void setAnchor(int row, int col) {
    anchor_row_ = row;
    anchor_col_ = col;
  }

  // The highlighted item, -1 for none.
  int selected() const { return selected_; }

 protected:
  bool dimsScreen() const override { return false; }
  void layout(Plane & screen, int rows, int cols) override;
  void keyDown(const KeyDown & e) override;
  void mouseDown(const MouseDown & e) override;
  void mouseMove(const MouseMove & e) override;
  void mouseUp(const MouseUp & e) override;
  void wheel(const Wheel & e) override;
  void close() override { plane_.reset(); }

 private:
  void init();
  bool choosable(int i) const { return i >= 0 && i < static_cast<int>(items_.size()) && !items_[static_cast<size_t>(i)].separator && items_[static_cast<size_t>(i)].enabled; }
  // The next item to land on from `from` in `step` (+1 or -1), round the ends; -1 where there is none.
  int step(int from, int step) const;
  // The item under a screen cell, -1 for none (a separator or the margin counts as none).
  int indexAt(int row, int col) const;
  bool inside(int row, int col) const { return row >= y_ && row < y_ + rows_ && col >= x_ && col < x_ + width_; }
  void select(int i);

  std::vector<Item> items_;
  int anchor_row_ = -1, anchor_col_ = 0;
  int selected_ = -1;
  int x_ = 0, y_ = 0, rows_ = 0, width_ = 0;
  bool pressed_inside_ = false;
  std::unique_ptr<Plane> plane_;
};

}  // namespace cellui
