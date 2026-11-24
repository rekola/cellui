#pragma once

#include <cellui/LineEditor.h>
#include <cellui/ModalDialog.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cellui {

// What a field holds, which decides how its text is checked.
enum class FieldKind {
  TEXT,    // any text
  NUMBER,  // a number: a point or a comma for the decimals, a sign; what parseFormNumber() reads
  DATE,    // a calendar date as YYYY-MM-DD; what parseFormDate() reads
};

// One labelled field of a FormModal.
struct FormField {
  std::string label;
  FieldKind kind = FieldKind::TEXT;
  std::string initial;
  // Shown quietly in the field while it is empty ("YYYY-MM-DD").
  std::string hint;
  // An empty field is an error ("Price is required").
  bool required = false;
  // A check of the text beyond the kind's own, which is run only when the kind's own passes and the field is not empty: the error
  // to show, or an empty string. ("Quantity must be above zero".)
  std::function<std::string(const std::string &)> check;
};

// A number as the form reads it: whole text only (a space around it is allowed), a point or a comma for the decimals (one of them), an
// optional sign; never the locale. None for anything else, and for a number that is not finite.
std::optional<double> parseFormNumber(const std::string & text);

struct FormDate {
  int year = 0, month = 0, day = 0;
};
// "2026-12-01": four digits, two, two, and a day the month has (leap years kept).
std::optional<FormDate> parseFormDate(const std::string & text);

// A modal form: labelled fields (text, number, date), each checked as it is typed, an error line, a summary line the program
// supplies and recomputes after every edit, and OK and Cancel.
//
//   Tab / Shift+Tab (Down / Up)  the next field, then OK, then Cancel, round
//   Enter                        accepts, from any field or OK; it does not while a field is wrong: the first wrong field is
//                                focused and its error shown. On Cancel it cancels.
//   Escape / Ctrl+G              cancels
//   Space on a button            presses it
//   a click                      on a field focuses it; on OK or Cancel presses it (the press and the release on the same one)
//   editing keys                 LineEditor's (C-a C-e C-k, arrows, Home, End, Backspace ...)
//
// showModal() returns 0 when accepted (read the values with text() and number()), kClosed when cancelled. It works the same in the
// terminal and in the window: it is cells and text on planes, with nothing the display has to draw itself.
class FormModal : public ModalDialog {
 public:
  FormModal(Widget & root, Plane & screen, EventLoop & loop, std::string title, std::vector<FormField> fields);

  // The line under the errors: told the text of every field, in order, it returns what to show ("Total 1 234.50 EUR"). Called after
  // every edit, and once when it is set.
  void setSummary(std::function<std::string(const std::vector<std::string> &)> summary);
  // A check of the fields together, run after every edit while every field is right, and on accept: the error to show (not
  // accepted while it says one), or an empty string. ("You hold only 10.")
  void setCheck(std::function<std::string(const std::vector<std::string> &)> check);

  size_t fieldCount() const { return fields_.size(); }
  std::string text(size_t field) const;
  void setText(size_t field, const std::string & text);
  std::vector<std::string> texts() const;
  // The value of a NUMBER field, none where it is empty or wrong.
  std::optional<double> number(size_t field) const;
  std::optional<FormDate> date(size_t field) const;

  // The field the keyboard is in; fieldCount() is OK and fieldCount() + 1 is Cancel.
  size_t focus() const { return focus_; }
  void setFocus(size_t focus);
  // The error line as shown, the summary line as shown.
  const std::string & error() const { return error_; }
  const std::string & summary() const { return summary_; }
  // The error of one field on its own (the kind's, the required one, the field's own check), empty for none.
  std::string fieldError(size_t field) const;
  // Whether every field is right and the check of the fields together passes.
  bool valid() const;

 protected:
  void layout(Plane & screen, int rows, int cols) override;
  void keyDown(const KeyDown & e) override;
  void paste(const Paste & e) override;
  void mouseDown(const MouseDown & e) override;
  void mouseMove(const MouseMove & e) override;
  void mouseUp(const MouseUp & e) override;
  void close() override { plane_.reset(); }

 private:
  struct Spot {  // a place on the dialog's plane
    int row = 0, col = 0, width = 0;
  };
  void changed();
  void accept();
  std::string formError() const;
  // Where the screen cell is: a field (its index), a button (fieldCount() or +1), or none (-1).
  int targetAt(int screen_row, int screen_col) const;

  std::string title_;
  std::vector<FormField> fields_;
  std::vector<LineEditor> editors_;
  std::function<std::string(const std::vector<std::string> &)> summary_fn_, check_fn_;
  std::string summary_, error_;
  size_t focus_ = 0;
  bool attempted_ = false;  // Enter was pressed while something was wrong: every wrong field shows its error now
  int pressed_ = -1;        // the button the left button is held on
  bool over_ = false;       // and the pointer is still over it
  int top_ = 0, left_ = 0;
  std::vector<Spot> field_spots_;
  Spot ok_, cancel_;
  std::unique_ptr<Plane> plane_;
};

}  // namespace cellui
