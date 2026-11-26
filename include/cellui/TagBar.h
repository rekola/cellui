#pragma once

#include <cellui/InputEvents.h>
#include <cellui/Signal.h>
#include <cellui/Widget.h>

#include <memory>
#include <string>
#include <vector>

namespace cellui {

// A row of clickable tags (chips), one of which is chosen: filters, presets,
// views. A click chooses the tag under the pointer; Left and Right move the
// choice while the bar has the keyboard focus, Up and Down a line. A tag that
// does not fit at the end of a line goes to the next one, as far as the bar is
// tall: `lines(cols)` says how many lines the tags need at a width, for an
// owner that gives the bar that many rows; a tag that is wider than the bar on
// its own is cut off at the right edge, and one that is on a line the bar has no
// row for is not drawn. Each tag is a plane of its own with a rounded border
// (Plane::setBorder(): a window draws it with rounded corners; a terminal shows
// the fill only).
//
// Like Table it uses the plane's own position in the parent, so it sits on
// the screen's root plane.
class TagBar : public Widget {
 public:
  explicit TagBar(Plane & parent);
  ~TagBar() override = default;

  void setTags(std::vector<std::string> tags);
  const std::vector<std::string> & tags() const { return tags_; }

  // The chosen tag, or -1 for none (nothing is chosen until select() or a
  // click). Emits `chosen` when it changes.
  int selected() const { return selected_; }
  void select(int index);

  // The tag drawn under local column `col` of the first line, or -1. For tests
  // and for owners that place something next to the bar.
  int tagAt(int col) const;
  // The same for local row `row` and column `col`.
  int tagAt(int row, int col) const;
  // The columns the bar needs for all its tags on one line.
  int preferredWidth() const;
  // The lines the tags take where the bar is `cols` wide, at least one.
  int lines(int cols) const;

  Signal<int> chosen;

  void render();

 protected:
  void onResize() override;

 private:
  // The line, first column and width of each tag, label and padding together, wrapped to `cols` columns (0: no wrapping).
  struct Chip {
    int row = 0, x = 0, width = 0;
  };
  std::vector<Chip> chips(int cols) const;
  std::vector<Chip> chips() const { return chips(getDim().second); }
  // The tag one line up (`delta` -1) or down (+1) from the chosen one, the nearest in column; -1 where there is none.
  int tagBeside(int delta) const;

  std::vector<std::string> tags_;
  std::vector<std::unique_ptr<Plane>> chip_planes_;
  int selected_ = -1;
  bool focused_ = false;
  std::vector<Connection> connections_;
};

}  // namespace cellui
