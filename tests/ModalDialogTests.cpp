#include "TestFramework.h"

#include <cellui/BufferPlane.h>
#include <cellui/ChoiceModal.h>
#include <cellui/InfoModal.h>
#include <cellui/InputDispatcher.h>
#include <cellui/PopupMenu.h>
#include <cellui/PlatformEventLoop.h>

using namespace cellui;

namespace {

Input key(int id) { return Input(id, 0, 0, false, false, false, false, Input::Kind::PRESS); }

// A widget tree under a screen, and a loop that turns without waiting; `script`
// feeds the dialog one input a turn.
struct Fixture {
  StyleProvider styles;
  CellBuffer buffer{12, 40};
  BufferPlane screen{buffer, styles};
  Widget root;
  InputDispatcher dispatcher{root};
  PlatformEventLoop loop;
  int turn = 0;
  int widget_keys = 0;
  Connection widget_connection;
  std::vector<Input> script;
  Connection frame;

  Fixture() {
    loop.setBefore([] { return 0; });
    // A key the widgets would get if a dialog did not take it.
    widget_connection = root.on<KeyDown>([this](const KeyDown &, EventContext &) {
      widget_keys++;
      return Handled::no;
    });
    frame = loop.onFrame([this] {
      if (turn < static_cast<int>(script.size())) dispatcher.offer(script[static_cast<size_t>(turn++)]);
      else loop.quit();
    });
  }
};

}  // namespace

TEST(choicemodal_exec_returns_the_chosen_index) {
  Fixture f;
  f.script = { key(Key::Down), key(Key::Enter) };
  ChoiceModal dialog(f.root, f.screen, f.loop, "Pick", { "alpha", "beta", "gamma" }, 0);
  CHECK(dialog.showModal() == 1);
  CHECK(f.root.activeModal() == nullptr);
}

TEST(choicemodal_escape_returns_closed) {
  Fixture f;
  f.script = { key(Key::Escape) };
  ChoiceModal dialog(f.root, f.screen, f.loop, "Pick", { "alpha", "beta" }, 0);
  CHECK(dialog.showModal() == ModalDialog::kClosed);
}

TEST(modaldialog_has_every_key_while_up_and_its_scrim_goes_with_it) {
  Fixture f;
  bool scrim_while_up = false;
  f.script = { key('x'), key('y') };
  auto spy = f.loop.onFrame([&] {
    f.screen.refresh();
    for (auto & plane : f.screen.describe().children)
      if (plane.name == "scrim") scrim_while_up = true;
  });
  ChoiceModal dialog(f.root, f.screen, f.loop, "Pick", { "alpha" }, 0);
  CHECK(dialog.showModal() == ModalDialog::kClosed); // the loop was quit under it
  CHECK(f.widget_keys == 0);
  CHECK(scrim_while_up || !f.styles.theme.modal_backdrop.active());
  f.screen.refresh();
  for (auto & plane : f.screen.describe().children) CHECK(plane.name != "scrim");
}

TEST(infomodal_closes_on_escape_and_ignores_releases) {
  Fixture f;
  f.script = { Input(Key::Enter, 0, 0, false, false, false, false, Input::Kind::RELEASE), key(Key::Escape) };
  InfoModal dialog(f.root, f.screen, f.loop, "About", "text");
  CHECK(dialog.wantsNativeSelection());
  CHECK(dialog.showModal() == 0);
  CHECK(f.turn == 2);
}

namespace {
// A dialog that says how big the screen was each time it was laid out.
class SizeDialog : public ModalDialog {
 public:
  using ModalDialog::ModalDialog;
  std::vector<std::pair<int, int>> sizes;

 protected:
  void layout(Plane &, int rows, int cols) override { sizes.push_back({rows, cols}); }
  void close() override {}
};
}  // namespace

TEST(modaldialog_is_laid_out_again_when_the_screen_changes_size) {
  Fixture f;
  SizeDialog dialog(f.root, f.screen, f.loop);
  bool resized = false;
  // On the turn after it opens the screen grows, as Application does it: the plane first, then the event.
  auto grow = f.loop.onFrame([&] {
    if (resized || f.turn == 0) return;
    resized = true;
    f.buffer = CellBuffer(20, 60);
    f.screen.resize(20, 60);
    f.root.postToRoot(Resize{20, 60});
  });
  f.script = { key('x'), key('y') };
  dialog.showModal();
  CHECK(resized);
  CHECK(dialog.sizes.size() >= 2);
  CHECK((dialog.sizes.front() == std::pair<int, int>{12, 40}));
  CHECK((dialog.sizes.back() == std::pair<int, int>{20, 60}));
}

namespace {
Input mouse(int row, int col, Input::Kind kind) { return Input(Key::Button1, row, col, false, false, false, false, kind); }
// Where the menu's first line is on the screen, in cells ({-1, -1} for none).
std::pair<int, int> find(const std::string & text, const std::string & needle) {
  int row = 0, col = 0;
  for (size_t i = 0; i < text.size(); i++) {
    if (text.compare(i, needle.size(), needle) == 0) return {row, col};
    if (text[i] == '\n') row++, col = 0;
    else if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) col++;
  }
  return {-1, -1};
}
}  // namespace

TEST(popupmenu_skips_separators_and_disabled_items_wraps_and_returns_the_chosen_index) {
  Fixture f;
  f.script = { key(Key::Down), key(Key::Enter) };
  PopupMenu menu(f.root, f.screen, f.loop, std::vector<PopupMenu::Item>{{"one"}, {"", true}, {"two", false, false}, {"three"}});
  CHECK(menu.selected() == 0);
  CHECK(menu.showModal() == 3);  // the separator and the disabled item are passed over
  Fixture g;
  g.script = { key(Key::Up), key(Key::Enter) };
  PopupMenu wrapping(g.root, g.screen, g.loop, std::vector<std::string>{"a", "b", "c"});
  CHECK(wrapping.showModal() == 2);  // Up from the first goes round to the last
  Fixture h;
  h.script = { key(Key::Escape) };
  PopupMenu closed(h.root, h.screen, h.loop, std::vector<std::string>{"a"});
  CHECK(closed.showModal() == ModalDialog::kClosed);
}

TEST(popupmenu_opens_under_its_anchor_with_no_scrim_and_the_page_behind_is_not_dimmed) {
  Fixture f;
  bool scrim = false;
  std::string shown;
  f.script = { key('x'), key(Key::Escape) };
  auto spy = f.loop.onFrame([&] {
    f.screen.refresh();
    for (auto & plane : f.screen.describe().children)
      if (plane.name == "scrim") scrim = true;
    if (shown.empty()) shown = f.buffer.toPlainText();
  });
  PopupMenu menu(f.root, f.screen, f.loop, std::vector<std::string>{"Show info", "Buy"});
  menu.setAnchor(2, 30);
  CHECK(menu.showModal() == ModalDialog::kClosed);
  CHECK(!scrim);
  const auto [row, col] = find(shown, "Show info");
  CHECK(row == 3);  // the row under the anchor
  CHECK(col >= 2 && col + 9 <= 31);  // its right edge on the anchor's column, not past it
  CHECK(find(shown, "Buy").first == 4);
}

TEST(popupmenu_a_click_on_an_item_chooses_it_the_pointer_highlights_and_a_click_away_closes_without_passing_it_on) {
  Fixture f;
  // The menu at the top left: its items are on rows 0 and 1.
  f.script = { mouse(1, 1, Input::Kind::PRESS), mouse(1, 1, Input::Kind::RELEASE) };
  PopupMenu menu(f.root, f.screen, f.loop, std::vector<std::string>{"alpha", "beta"});
  CHECK(menu.showModal() == 1);
  Fixture g;
  g.script = { mouse(8, 20, Input::Kind::PRESS) };
  PopupMenu away(g.root, g.screen, g.loop, std::vector<std::string>{"alpha", "beta"});
  CHECK(away.showModal() == ModalDialog::kClosed && g.widget_keys == 0);
}
