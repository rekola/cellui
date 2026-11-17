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
// choice while the bar has the keyboard focus. Tags that do not fit are cut
// off at the right edge. Each tag is a plane of its own with a rounded border
// (a Band's, so a window draws it with rounded corners and a terminal with
// kitty graphics with square ones; other displays show the fill only).
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

  // The tag drawn under local column `col`, or -1. For tests and for owners
  // that place something next to the bar.
  int tagAt(int col) const;
  // The columns the bar needs for all its tags.
  int preferredWidth() const;

  Signal<int> chosen;

  void render();

 protected:
  void onResize() override;

 private:
  // The first column and width of each tag, label and padding together.
  struct Chip {
    int x = 0, width = 0;
  };
  std::vector<Chip> chips() const;

  std::vector<std::string> tags_;
  std::vector<std::unique_ptr<Plane>> chip_planes_;
  int selected_ = -1;
  bool focused_ = false;
  std::vector<Connection> connections_;
};

}  // namespace cellui
