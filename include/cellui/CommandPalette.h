#pragma once

#include <cellui/ChoiceList.h>
#include <cellui/Input.h>
#include <cellui/InputEvents.h>
#include <cellui/Plane.h>

#include <memory>
#include <string>
#include <vector>

namespace cellui {

// One command the palette can run.
struct PaletteEntry {
  std::string name;
  std::string description;
  // How it is bound in the current keymap, "" if it is not.
  std::string binding;
};

// A modal command prompt anchored to the bottom of the screen, where the
// status line is: the query on the last row, the selected command's
// description above it, and above that the commands that fuzzy-match best
// first with their keybindings, the list growing upward as more match. Typing edits the query; Up/Down (and the Emacs-style
// next/previous-line keys) move; Tab completes the query as far as every
// command starting with it agrees, the way Emacs completes a command name; Enter or a
// click chooses; Escape or a click outside cancels. The caller runs the chosen command.
//
// A program opens it with Application::openCommandPalette(), which routes the
// events to it, lays it out and runs the choice; the calls below are what the
// application uses, and what this widget's own tests drive.
class CommandPalette {
 public:
  enum class Result { NONE,
                      CHOSEN,
                      CANCELLED };

  bool isOpen() const { return open_; }

  // Nothing is drawn until show().
  void open(std::vector<PaletteEntry> entries);
  void close();
  void raise() {
    if (plane_) plane_->moveToTop();
    if (list_plane_) list_plane_->moveToTop();
  }
  // Where the display scrolls smoothly (DisplayInfo::smooth_scroll) the list eases to a new scroll position
  // instead of jumping to it. The application calls tick() for each frame with the seconds since the last
  // (Application does) and draws again while it says it is still moving.
  bool animating() const { return open_ && list_.animating(); }
  bool tick(double seconds) {
    list_.tick(seconds);
    return list_.animating();
  }

  // (Re)draws the panel under `parent`, across the bottom of a screen of the
  // given size. Call again after a resize or any change.
  void show(Plane & parent, int screen_rows, int screen_cols);

  // Feeds one input event; the palette swallows everything, so the caller
  // only needs the result.
  // Keys act on the press. A command is chosen by the release of the left button over it, after a press on a
  // command (shown pressed, following the pointer while the button is held); a press outside the panel and a
  // release outside it cancels. A release with no press seen before it is a whole click.
  // The wheel scrolls the list and leaves the selection where it is, even out of view.
  Result key(const KeyDown & e);
  Result press(int screen_y, int screen_x);
  Result drag(int screen_y, int screen_x);
  Result click(int screen_y, int screen_x);
  Result wheel(bool up);

  // The selected command, nullptr when nothing matches.
  const PaletteEntry * chosen() const;
  const std::string & query() const { return query_; }
  // What the last Tab had to say ("[No match]"), "" if nothing.
  const std::string & note() const { return note_; }
  // The names shown, best match first.
  std::vector<std::string> shownNames() const;

 private:
  struct Shown {
    size_t entry;
    std::vector<int> positions;
  };
  void refilter();
  void complete();
  int visibleRows() const { return rows_ - 2; }
  // The command shown under a screen cell, -1 for none; whether a cell is in the panel.
  int indexAt(int screen_y, int screen_x);
  bool inPanel(int screen_y, int screen_x) const;

  bool open_ = false;
  std::unique_ptr<Plane> plane_;
  // The commands' rows, a plane of their own over the panel's, so the display can shift them by the part of a row
  // the list is scrolled into while it eases.
  std::unique_ptr<Plane> list_plane_;
  std::vector<PaletteEntry> entries_;
  std::string query_;
  // "[No match]" or "[Sole completion]" after a Tab, until the next edit.
  std::string note_;
  std::vector<Shown> shown_;
  ChoiceList list_;
  // The left button is down: on a command, or outside the panel.
  bool pressing_ = false, started_on_command_ = false, started_outside_ = false;
  int y_ = 0, x_ = 0, rows_ = 0, width_ = 0;
};

}  // namespace cellui
