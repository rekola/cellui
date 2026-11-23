#pragma once

#include <cellui/ScrollBody.h>
#include <cellui/Widget.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cellui/Input.h>
#include <cellui/InputEvents.h>

namespace cellui {

// A modal list on a raised panel (no border) the person picks one entry from: the terminal
// rendering of a choice dialog. Up/Down/PageUp/PageDown/Home/End (and
// the Emacs-style next/previous-line keys) move, Enter or a click chooses,
// Escape or a click outside cancels; the mouse wheel scrolls.
class ChoiceDialog {
 public:
  enum class Result { NONE,
                      CHOSEN,
                      CANCELLED };

  bool isOpen() const { return plane_ != nullptr || !labels_.empty(); }

  // `current` is marked (the entry already in use) and is where the selection
  // starts, -1 for none. Nothing is drawn until show().
  void open(std::string title, std::vector<std::string> labels, int current);
  void close();
  // How high it floats (Plane::setElevation()): the theme's overlay
  // elevation unless set; a modal dialog is set to the modal one.
  void setElevation(float elevation) {
    elevation_ = elevation;
    if (plane_ && elevation_) plane_->setElevation(*elevation_);
  }
  // Above every plane under the same parent.
  void raise() {
    if (plane_) plane_->moveToTop();
    body_.raise();
  }
  // Whether the list is still moving (smooth scrolling): another show() is due.
  bool animating() const;

  // (Re)draws the box under `parent`, centred on a screen of the given size.
  // Call again after a resize or any change of selection.
  void show(Plane & parent, int screen_rows, int screen_cols);

  // Feed one event; the dialog swallows everything, so the caller only needs
  // the result. An entry is chosen by the release of the left button over it, after a press on an entry (shown
  // pressed, following the pointer while the button is held); a press outside the box and a release outside it
  // cancels. A release with no press seen before it is a whole click. The wheel scrolls the list and leaves the
  // selection where it is, even out of view.
  Result key(const KeyDown & e);
  Result press(int row, int col);
  Result drag(int row, int col);
  Result click(int row, int col);
  Result wheel(bool up);

  int selected() const { return body_.selected(); }

 private:
  int visibleRows() const { return rows_ - 5; }  // see kChromeRows
  // Whether a cell is in the box.
  bool inBox(int y, int x) const;

  // The box with its title and hint; the entries are a ScrollBody over it, so they can scroll by parts of a row
  // under a cursor band.
  std::unique_ptr<Plane> plane_;
  std::optional<float> elevation_;
  ScrollBody body_;
  std::string title_;
  std::vector<std::string> labels_;
  int current_ = -1;
  // Where the box sits and how big it is, as of the last show().
  int y_ = 0, x_ = 0, rows_ = 0, width_ = 0;
};

}  // namespace cellui
