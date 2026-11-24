#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/FormModal.h>
#include <cellui/InputDispatcher.h>
#include <cellui/PlatformEventLoop.h>

using namespace cellui;

namespace {

Input press(int id, unsigned mods = 0) { return keyInput(id, mods, Input::Kind::PRESS); }
Input mouse(int row, int col, Input::Kind kind) { return Input(Key::Button1, row, col, false, false, false, false, kind); }

// A widget tree under a screen and a loop that turns without waiting; each turn runs one step (a key, a click, a look at the
// screen) and the loop ends when they run out.
struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{24, 70};
  BufferPlane screen{buffer, styles};
  Widget root;
  InputDispatcher dispatcher{root};
  PlatformEventLoop loop;
  size_t turn = 0;
  int widget_keys = 0;
  Connection widget_connection;
  std::vector<std::function<void()>> steps;
  Connection frame;

  Fixture() {
    loop.setBefore([] { return 0; });
    widget_connection = root.on<KeyDown>([this](const KeyDown &, EventContext &) {
      widget_keys++;
      return Handled::no;
    });
    frame = loop.onFrame([this] {
      if (turn < steps.size()) steps[turn++]();
      else loop.quit();
    });
  }
  void key(int id, unsigned mods = 0) {
    steps.push_back([this, id, mods] { dispatcher.offer(press(id, mods)); });
  }
  void type(const std::string & s) {
    for (char c : s) key(c);
  }
  void paste(const std::string & s) {
    steps.push_back([this, s] { dispatcher.paste(Paste{s}); });
  }
  void click(int row, int col) {
    steps.push_back([this, row, col] {
      dispatcher.offer(mouse(row, col, Input::Kind::PRESS));
      dispatcher.offer(mouse(row, col, Input::Kind::RELEASE));
    });
  }
  std::string screenText() {
    screen.refresh();
    return buffer.toPlainText();
  }
  // Where a text first is on the screen: {row, column}, or {-1, -1}.
  std::pair<int, int> find(const std::string & needle) {
    const std::string text = screenText();
    int row = 0, col = 0;
    for (size_t i = 0; i < text.size(); i++) {
      if (text.compare(i, needle.size(), needle) == 0) return {row, col};
      if (text[i] == '\n') row++, col = 0;
      else if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) col++;
    }
    return {-1, -1};
  }
};

std::vector<FormField> orderFields() {
  FormField quantity{"Quantity", FieldKind::NUMBER, "", "", true};
  quantity.check = [](const std::string & t) { return parseFormNumber(t) && *parseFormNumber(t) <= 0 ? std::string("Quantity must be above zero") : std::string(); };
  return {quantity, {"Price", FieldKind::NUMBER, "", "", true}, {"Date", FieldKind::DATE, "2026-12-01", "YYYY-MM-DD", true}, {"Note", FieldKind::TEXT, "", "", false}};
}

}  // namespace

TEST(form_numbers_take_a_point_or_a_comma_and_nothing_else) {
  CHECK(parseFormNumber("12") == 12.0 && parseFormNumber("4.25") == 4.25 && parseFormNumber("4,25") == 4.25);
  CHECK(parseFormNumber(" -3.5 ") == -3.5 && parseFormNumber("+2") == 2.0 && parseFormNumber("0.001") == 0.001);
  for (const char * bad : {"", " ", "abc", "1.2.3", "1,2,3", "1,2.3", "1 2", "--1", "+", "1e", "inf", "nan", "12x", ".", ","}) CHECK(!parseFormNumber(bad));
}

TEST(form_dates_are_calendar_dates_as_year_month_day) {
  const auto d = parseFormDate("2026-12-01");
  CHECK(d && d->year == 2026 && d->month == 12 && d->day == 1);
  CHECK(parseFormDate("2024-02-29") && !parseFormDate("2026-02-29") && !parseFormDate("1900-02-29") && parseFormDate("2000-02-29"));
  for (const char * bad : {"", "2026-13-01", "2026-00-10", "2026-04-31", "2026-4-1", "26-04-01", "2026/04/01", "2026-04-1a", "2026-04-011", "0000-01-01"}) CHECK(!parseFormDate(bad));
}

TEST(form_typing_fills_the_focused_field_tab_moves_on_and_enter_accepts) {
  Fixture f;
  FormModal form(f.root, f.screen, f.loop, "Buy Nokia", orderFields());
  f.type("12");
  f.key(Key::Tab);
  f.type("4.5");
  f.key(Key::Tab);  // the date, which has its initial text
  f.key(Key::Tab);
  f.type("a note");
  f.key(Key::Enter);
  CHECK(form.showModal() == 0);
  CHECK(form.texts() == (std::vector<std::string>{"12", "4.5", "2026-12-01", "a note"}));
  CHECK(form.number(0) == 12.0 && form.number(1) == 4.5 && form.date(2) && form.date(2)->month == 12 && !form.number(3));
  CHECK(f.widget_keys == 0);  // the dialog took every key
  CHECK(f.root.activeModal() == nullptr);
}

TEST(form_a_paste_goes_into_the_focused_field) {
  Fixture f;
  FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
  f.paste("12");
  f.key(Key::Tab);
  f.paste("4.5");
  f.key(Key::Enter);
  CHECK(form.showModal() == 0);
  CHECK(form.texts()[0] == "12" && form.texts()[1] == "4.5");
}

TEST(form_enter_does_not_accept_while_a_field_is_wrong_and_shows_which) {
  Fixture f;
  FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
  f.key(Key::Tab);  // leaves the empty required quantity
  f.type("4");
  f.key(Key::Enter);  // refused: the quantity is empty. Not accepted, so the script ends the loop
  CHECK(form.showModal() == ModalDialog::kClosed);
  CHECK(form.focus() == 0 && form.error() == "Quantity is required");
  CHECK(!form.valid());
  // Fixed, it is accepted.
  Fixture g;
  FormModal again(g.root, g.screen, g.loop, "Buy", orderFields());
  g.key(Key::Enter);
  g.type("3");
  g.key(Key::Tab);
  g.type("4");
  g.key(Key::Enter);
  CHECK(again.showModal() == 0 && again.number(0) == 3.0 && again.number(1) == 4.0);
}

TEST(form_each_field_is_checked_as_it_is_typed_and_the_error_line_says_what_is_wrong) {
  Fixture f;
  FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
  f.type("abc");
  f.steps.push_back([&] { CHECK(form.error() == "Quantity is not a number"); });
  f.key(Key::Backspace);
  f.key(Key::Backspace);
  f.key(Key::Backspace);
  f.steps.push_back([&] { CHECK(form.error().empty()); });  // empty and not yet left: no complaint
  f.type("0");
  f.steps.push_back([&] { CHECK(form.error() == "Quantity must be above zero" && !form.valid()); });  // the field's own check
  f.key(Key::Backspace);
  f.type("2");
  f.steps.push_back([&] { CHECK(form.error().empty() && form.number(0) == 2.0); });
  f.key(Key::Tab);
  f.key(Key::Tab);
  f.key('k', kModCtrl);  // kills the date to the end of the line: it has the cursor at the end, so nothing
  f.key(Key::Home);
  f.key('k', kModCtrl);
  f.steps.push_back([&] { CHECK(form.text(2).empty()); });
  f.type("2026-02-30");
  f.steps.push_back([&] { CHECK(form.error() == "Date is not a date (YYYY-MM-DD)"); });
  f.steps.push_back([&] { std::string screen = f.screenText(); CHECK(screen.find("Date is not a date (YYYY-MM-DD)") != std::string::npos); });
  CHECK(form.showModal() == ModalDialog::kClosed);
  CHECK(form.fieldError(0).empty() && !form.fieldError(2).empty() && form.fieldError(3).empty());
}

TEST(form_the_summary_follows_every_edit_and_the_check_of_the_fields_together_stops_an_accept) {
  Fixture f;
  FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
  form.setSummary([](const std::vector<std::string> & t) {
    const auto q = parseFormNumber(t[0]), p = parseFormNumber(t[1]);
    return q && p ? "Total " + std::to_string(static_cast<int>(*q * *p)) : std::string("Total -");
  });
  form.setCheck([](const std::vector<std::string> & t) {
    const auto q = parseFormNumber(t[0]);
    return q && *q > 10 ? std::string("You hold only 10") : std::string();
  });
  CHECK(form.summary() == "Total -");
  f.type("20");
  f.key(Key::Tab);
  f.type("3");
  f.steps.push_back([&] { CHECK(form.summary() == "Total 60" && form.error() == "You hold only 10"); });
  f.steps.push_back([&] { CHECK(f.screenText().find("Total 60") != std::string::npos && f.screenText().find("You hold only 10") != std::string::npos); });
  f.key(Key::Enter);  // the check says no
  CHECK(form.showModal() == ModalDialog::kClosed);
  CHECK(!form.valid() && form.error() == "You hold only 10");
  // Made right, it goes.
  Fixture g;
  FormModal ok(g.root, g.screen, g.loop, "Buy", orderFields());
  ok.setCheck([](const std::vector<std::string> & t) { return t[0] == "20" ? std::string("no") : std::string(); });
  g.type("20");
  g.key(Key::Tab);
  g.type("3");
  g.key(Key::Enter);  // the check says no, and the keyboard stays where it was
  g.steps.push_back([&] { CHECK(ok.focus() == 1 && ok.error() == "no"); });
  g.key(Key::Tab, kModShift);
  g.key(Key::Backspace);
  g.type("5");  // 25
  g.key(Key::Enter);
  CHECK(ok.showModal() == 0 && ok.number(0) == 25.0 && ok.number(1) == 3.0);
}

TEST(form_escape_and_ctrl_g_cancel) {
  for (int variant = 0; variant < 2; variant++) {
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    f.type("1");
    if (variant == 0) f.key(Key::Escape);
    else f.key('g', kModCtrl);
    f.key(Key::Enter);  // never reached
    CHECK(form.showModal() == ModalDialog::kClosed);
    CHECK(f.turn == 2);
  }
}

TEST(form_tab_walks_the_fields_then_ok_then_cancel_and_round_and_shift_tab_goes_back) {
  Fixture f;
  FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
  std::vector<size_t> seen;
  auto note = [&] { f.steps.push_back([&] { seen.push_back(form.focus()); }); };
  note();
  for (int i = 0; i < 6; i++) {
    f.key(Key::Tab);
    note();
  }
  f.key(Key::Tab, kModShift);
  note();
  f.key(Key::Down);
  note();
  f.key(Key::Up);
  note();
  CHECK(form.showModal() == ModalDialog::kClosed);
  CHECK(seen == (std::vector<size_t>{0, 1, 2, 3, 4, 5, 0, 5, 0, 5}));  // fields 0-3, OK 4, Cancel 5
}

TEST(form_enter_on_cancel_cancels_and_on_ok_accepts_and_space_presses_a_button) {
  {
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    form.setText(0, "1");
    form.setText(1, "2");
    form.setFocus(5);
    f.key(Key::Enter);
    CHECK(form.showModal() == ModalDialog::kClosed);
  }
  {
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    form.setText(0, "1");
    form.setText(1, "2");
    form.setFocus(4);
    f.key(' ');
    CHECK(form.showModal() == 0);
  }
  {
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    form.setFocus(5);
    f.key(' ');
    CHECK(form.showModal() == ModalDialog::kClosed);
  }
}

TEST(form_editing_keys_are_the_line_editors) {
  Fixture f;
  std::vector<FormField> fields = {{"Name", FieldKind::TEXT, "hello world"}};
  FormModal form(f.root, f.screen, f.loop, "Name", fields);
  f.key('a', kModCtrl);
  f.key('d', kModCtrl);  // deletes the h
  f.key(Key::End);
  f.key(Key::Backspace);
  f.type("D!");
  f.key(Key::Left);
  f.key(Key::Left);
  f.type("_");
  CHECK(form.showModal() == ModalDialog::kClosed);
  CHECK(form.text(0) == "ello worl_D!");
}

TEST(form_is_drawn_with_its_title_labels_hints_and_buttons_and_the_input_is_the_dialogs_alone) {
  Fixture f;
  FormModal form(f.root, f.screen, f.loop, "Buy Nokia", {{"Quantity", FieldKind::NUMBER, "", "", true}, {"Date", FieldKind::DATE, "", "YYYY-MM-DD", true}});
  f.steps.push_back([&] {
    const std::string screen = f.screenText();
    for (const char * s : {"Buy Nokia", "Quantity", "Date", "YYYY-MM-DD", "OK", "Cancel", "Tab: next"}) CHECK(screen.find(s) != std::string::npos);
  });
  f.steps.push_back([&] { f.dispatcher.offer(press('x')); });
  CHECK(form.showModal() == ModalDialog::kClosed);
  CHECK(f.widget_keys == 0 && form.text(0) == "x");
  f.screen.refresh();
  for (auto & plane : f.screen.describe().children) CHECK(plane.name != "scrim");
}

TEST(form_a_click_focuses_a_field_and_ok_and_cancel_are_pressed_by_the_mouse) {
  {
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    form.setText(0, "1");
    form.setText(1, "2");
    f.steps.push_back([&] {
      const auto at = f.find("Price");
      CHECK(at.first >= 0);
      f.dispatcher.offer(mouse(at.first, at.second + 14, Input::Kind::PRESS));
      f.dispatcher.offer(mouse(at.first, at.second + 14, Input::Kind::RELEASE));
    });
    CHECK(form.showModal() == ModalDialog::kClosed);
    CHECK(form.focus() == 1);
  }
  {
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    form.setText(0, "1");
    form.setText(1, "2");
    f.steps.push_back([&] {
      const auto at = f.find(" OK ");
      CHECK(at.first >= 0);
      f.dispatcher.offer(mouse(at.first, at.second + 1, Input::Kind::PRESS));
      f.dispatcher.offer(mouse(at.first, at.second + 1, Input::Kind::RELEASE));
    });
    CHECK(form.showModal() == 0);
  }
  {
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    form.setText(0, "1");
    form.setText(1, "2");
    f.steps.push_back([&] {
      const auto at = f.find(" Cancel");
      CHECK(at.first >= 0);
      f.dispatcher.offer(mouse(at.first, at.second + 2, Input::Kind::PRESS));
      f.dispatcher.offer(mouse(at.first, at.second + 2, Input::Kind::RELEASE));
    });
    CHECK(form.showModal() == ModalDialog::kClosed);
  }
  {
    // Pressed on OK and released elsewhere: nothing is pressed.
    Fixture f;
    FormModal form(f.root, f.screen, f.loop, "Buy", orderFields());
    form.setText(0, "1");
    form.setText(1, "2");
    f.steps.push_back([&] {
      const auto at = f.find(" OK ");
      f.dispatcher.offer(mouse(at.first, at.second + 1, Input::Kind::PRESS));
      f.dispatcher.offer(mouse(at.first + 3, at.second + 1, Input::Kind::RELEASE));
    });
    CHECK(form.showModal() == ModalDialog::kClosed);  // the script ran out; it did not accept
    CHECK(f.turn == 1);
  }
}
