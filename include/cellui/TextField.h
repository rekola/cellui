#pragma once

#include <cellui/Input.h>
#include <cellui/InputEvents.h>
#include <cellui/LineEditor.h>

#include <string>

namespace cellui {

class Plane;

// The one-line text input every cellui field is: the status line's prompt, an
// InlineEditor over a widget's cells and each field of a FormModal. It owns
// the editing (a LineEditor: typing and the Emacs keys), turns what is
// pasted into one line, and draws the visible part. Where its keys and its
// pastes come from is the owner's: a reader on a plane gets them from the
// application's key and paste routing, a form's field from its dialog.
class TextField {
 public:
  void setText(const std::string & text) { editor_.setText(text); }
  std::string text() const { return editor_.text(); }

  // Typing and the editing keys; false for a key the field leaves to its owner.
  bool key(const KeyDown & e) { return editor_.key(e); }
  // Inserts pasted text at the cursor: a line break, tab or other control character becomes
  // a space (a field is one line), and a CR LF pair one space.
  void paste(const std::string & text);

  // Writes the visible part of the text at (row, col) in the plane's current style over `width`
  // columns, and says where the cursor is in it.
  struct Drawn {
    std::string text;
    int cursor_column = 0;
  };
  Drawn draw(Plane & plane, int row, int col, int width);
  // The character under the cursor in what draw() drew (a space past the end), for a field that paints its own cursor cell.
  static std::string cellUnder(const Drawn & drawn);

 private:
  LineEditor editor_;
};

}  // namespace cellui
