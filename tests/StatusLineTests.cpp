#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/InputDispatcher.h>
#include <cellui/StatusLine.h>

using namespace cellui;

namespace {

KeyDown key(int id, bool ctrl = false, bool alt = false, bool shift = false) {
  return InputDispatcher::keyDownFor(Input(id, 0, 0, alt, shift, ctrl, false, Input::Kind::PRESS));
}

struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{3, 30};
  BufferPlane root{buffer, styles};
  StatusLine line{root};
  Fixture() {
    line.resize(1, 30);
    line.move(2, 0);
  }
  void type(const std::string & s) {
    for (char c : s) CHECK(line.keyDown(key(c)));
  }
};

}  // namespace

TEST(statusline_shows_a_message_and_holds_one_set_while_the_prompt_is_open) {
  Fixture f;
  f.line.setMessage("Saved");
  f.root.refresh();
  CHECK(f.buffer.toPlainText().find("Saved") != std::string::npos);
  f.line.showPrompt("Name: ", [](const std::string &) { });
  f.line.setMessage("Later");
  f.root.refresh();
  CHECK(f.buffer.toPlainText().find("Later") == std::string::npos);
  f.line.cancelReader();
  f.root.refresh();
  CHECK(f.buffer.toPlainText().find("Later") != std::string::npos);  // shown once the prompt closed
}

TEST(statusline_prompt_submits_on_enter_and_cancels_on_ctrl_g) {
  Fixture f;
  std::string got = "none";
  f.line.showPrompt("Name: ", [&](const std::string & s) { got = s; });
  CHECK(f.line.isReaderActive());
  f.type("abc");
  CHECK(f.line.keyDown(key(Key::Enter)));
  CHECK(got == "abc" && !f.line.isReaderActive());
  got = "none";
  f.line.showPrompt("Name: ", [&](const std::string & s) { got = s; });
  f.type("x");
  CHECK(f.line.keyDown(key('g', true)));
  CHECK(got == "none" && !f.line.isReaderActive());
}

TEST(statusline_tab_completes_a_candidate_set_and_says_when_nothing_matches) {
  Fixture f;
  std::string got;
  auto candidates = [](const std::string & text) {
    std::set<std::string> all = {"sort-by-name", "sort-by-price", "quit"}, out;
    for (auto & n : all) {
      if (n.compare(0, text.size(), text) == 0) out.insert(n);
    }
    return out;
  };
  f.line.showPromptWithCompletion("M-x ", [&](const std::string & s) { got = s; }, candidates);
  f.type("so");
  CHECK(f.line.keyDown(key(Key::Tab)));
  f.line.keyDown(key('x'));  // typed on, so the next Tab starts again from "sort-by-x"
  CHECK(f.line.keyDown(key(Key::Tab)));  // no match for "sort-by-x": the prompt stays open
  CHECK(f.line.isReaderActive());
  f.line.keyDown(key(Key::Backspace));
  CHECK(f.line.keyDown(key(Key::Enter)));
  CHECK(got == "sort-by-");  // the common prefix, which Enter submitted as typed
}

TEST(statusline_command_prompt_shortcut_follows_the_keymap_profile) {
  Fixture f;
  bool opened = false;
  f.line.setCommandPaletteCallback([&] { opened = true; });
  CHECK(!f.line.keyDown(key('a')));
  CHECK(f.line.keyDown(key('x', false, true)));  // Meta-x on the terminal keymap
  CHECK(opened);
  CHECK(!f.line.isCommandPromptShortcut(key('p', true, false, true)));  // Ctrl+Shift+P is the desktop's
}
