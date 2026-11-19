#include "TestFramework.h"

#include <cellui/CommandRegistry.h>
#include <cellui/KeyStroke.h>
#include <cellui/Keymap.h>

using namespace cellui;

TEST(keystroke_pack_tells_modifiers_apart) {
  CHECK(KeyStroke::pack('x', true, false, false, false) != KeyStroke::pack('x', false, false, false, false));
  CHECK(KeyStroke::pack('x', false, true, false, false) != KeyStroke::pack('x', false, false, true, false));
  CHECK(KeyStroke::pack('x', false, false, false, true) != KeyStroke::pack('y', false, false, false, true));
}

TEST(registry_executes_and_completes_by_prefix) {
  CommandRegistry reg;
  int ran = 0;
  reg.define("set-mark", [&] { ran++; });
  reg.define("set-mode", [] {});
  CHECK(reg.has("set-mark"));
  CHECK(reg.execute("set-mark"));
  CHECK(ran == 1);
  CHECK(!reg.execute("nope"));
  CHECK(reg.matching("set-ma").size() == 1);
  CHECK(reg.matching("set-").size() == 2);
  reg.undefine("set-mark");
  CHECK(!reg.has("set-mark"));
  CHECK(reg.matching("set-").size() == 1);
}

TEST(keymap_binds_prefixes_and_checks_names) {
  CommandRegistry reg;
  reg.define("quit", [] {});
  Keymap map;
  auto cx = KeyStroke::pack('x', true, false, false, false);
  auto cc = KeyStroke::pack('c', true, false, false, false);
  map.bindPrefixed(cx, cc, "quit");
  CHECK(map.isPrefix(cx, KeymapProfile::TERMINAL));
  CHECK(!map.isPrefix(cc, KeymapProfile::TERMINAL));
  CHECK(map.lookup(cx, KeymapProfile::TERMINAL) == nullptr);
  CHECK(map.lookupPrefixed(cx, cc, KeymapProfile::TERMINAL) && *map.lookupPrefixed(cx, cc, KeymapProfile::TERMINAL) == "quit");
  CHECK(map.allBoundIn(reg));
  map.bind(cc, "typo");
  CHECK(!map.allBoundIn(reg));
}

TEST(keymap_profiles_keep_their_own_bindings) {
  using P = KeymapProfile;
  Keymap map;
  auto cy = KeyStroke::pack('y', true, false, false, false);
  auto cx = KeyStroke::pack('x', true, false, false, false);
  auto cs = KeyStroke::pack('s', true, false, false, false);
  map.bind(cy, "shared");
  map.bind(P::TERMINAL, cy, "yank");
  map.bind(P::DESKTOP, cx, "cut");
  map.bindPrefixed(P::TERMINAL, cx, cs, "save");
  // A profile's own binding wins over a shared one; the other profile still
  // sees the shared one.
  CHECK(*map.lookup(cy, P::TERMINAL) == "yank");
  CHECK(*map.lookup(cy, P::DESKTOP) == "shared");
  // A shortcut bound for one profile is unbound in the other, prefixes included.
  CHECK(map.lookup(cx, P::TERMINAL) == nullptr);
  CHECK(*map.lookup(cx, P::DESKTOP) == "cut");
  CHECK(map.isPrefix(cx, P::TERMINAL));
  CHECK(!map.isPrefix(cx, P::DESKTOP));
  CHECK(map.lookupPrefixed(cx, cs, P::DESKTOP) == nullptr);
}

TEST(keystroke_packs_an_input_like_its_fields) {
  Input in('x', 0, 0, false, false, true, false);
  CHECK(KeyStroke::pack(in) == KeyStroke::pack('x', true, false, false, false));
  CHECK(in.getTrueCaseId() == 'x');
  Input upper('x', 0, 0, false, true, false, false, Input::Kind::PRESS, 'X');
  CHECK(upper.getTrueCaseId() == 'X' && upper.getKind() == Input::Kind::PRESS);
}

TEST(keystroke_parse_reads_both_spellings_back) {
  using KeyStroke::pack;
  using KeyStroke::parse;
  CHECK(parse("q") == std::vector<uint64_t>{pack('q', false, false, false, false)});
  CHECK(parse("C-x C-c") == (std::vector<uint64_t>{pack('x', true, false, false, false), pack('c', true, false, false, false)}));
  CHECK(parse("M-x") == std::vector<uint64_t>{pack('x', false, true, false, false)});
  CHECK(parse("C-S-p") == std::vector<uint64_t>{pack('p', true, false, true, false)});
  CHECK(parse("Ctrl+Shift+P") == std::vector<uint64_t>{pack('p', true, false, true, false)});
  CHECK(parse("ctrl+q") == std::vector<uint64_t>{pack('q', true, false, false, false)});
  CHECK(parse("Ctrl++") == std::vector<uint64_t>{pack('+', true, false, false, false)});
  CHECK(parse("C--") == std::vector<uint64_t>{pack('-', true, false, false, false)});
  CHECK(parse("RET") == std::vector<uint64_t>{pack(Key::Enter, false, false, false, false)});
  CHECK(parse("Alt+Enter") == std::vector<uint64_t>{pack(Key::Enter, false, true, false, false)});
  CHECK(parse("C-S-Up") == std::vector<uint64_t>{pack(Key::Up, true, false, true, false)});
  CHECK(parse("F12") == std::vector<uint64_t>{pack(Key::F12, false, false, false, false)});
  CHECK(parse("ä") == std::vector<uint64_t>{pack(0xe4, false, false, false, false)});
  CHECK(parse("<") == std::vector<uint64_t>{pack('<', false, false, false, false)});
  CHECK(parse("Q") == std::vector<uint64_t>{pack('Q', false, false, false, false)});  // a bare capital is the character
  CHECK(parse("Hyper+x").empty());
  CHECK(parse("X-y").empty());
  CHECK(parse("Nonesuch").empty());
  CHECK(parse("").empty());
  // describe() reads back as itself.
  for (const char * keys : {"C-x C-s", "M-w", "C-S-Up", "q", "DEL", "F5"}) CHECK(KeyStroke::describe(parse(keys), KeymapProfile::TERMINAL) == keys);
  for (const char * keys : {"Ctrl+S", "Ctrl+Shift+Z", "Alt+Enter", "PageDown"}) CHECK(KeyStroke::describe(parse(keys), KeymapProfile::DESKTOP) == keys);
}

TEST(keymap_binds_keys_as_written_and_names_the_simplest_binding) {
  Keymap map;
  CHECK(map.bind("q", "quit"));
  CHECK(map.bind(KeymapProfile::TERMINAL, "C-x C-c", "quit"));
  CHECK(map.bind(KeymapProfile::DESKTOP, "Ctrl+Q", "quit"));
  CHECK(*map.lookup(KeyStroke::pack('q', false, false, false, false), KeymapProfile::DESKTOP) == "quit");
  CHECK(*map.lookupPrefixed(KeyStroke::pack('x', true, false, false, false), KeyStroke::pack('c', true, false, false, false), KeymapProfile::TERMINAL) == "quit");
  CHECK(map.bindingFor("quit", KeymapProfile::TERMINAL) == "q");
  map.bind("C-s", "save");
  map.bind("C-x C-s", "save");
  CHECK(map.bindingFor("save", KeymapProfile::TERMINAL) == "C-s");
  CHECK(map.bindingFor("save", KeymapProfile::DESKTOP) == "Ctrl+S");
  CHECK(map.bindingFor("nothing", KeymapProfile::TERMINAL).empty());
}

TEST(command_registry_keeps_descriptions_and_indexes_a_redefined_name_once) {
  CommandRegistry registry;
  int runs = 0;
  registry.define("b-command", "Does b", [&] { runs += 1; });
  registry.define("a-command", [&] {});
  registry.define("b-command", [&] { runs += 10; });
  CHECK(registry.matching("b").size() == 1u);
  CHECK(registry.names() == (std::vector<std::string>{"a-command", "b-command"}));
  CHECK(registry.execute("b-command") && runs == 10);
  CHECK(registry.description("b-command") == "Does b");
  CHECK(registry.description("a-command").empty());
  registry.undefine("b-command");
  CHECK(registry.description("b-command").empty() && registry.names().size() == 1u);
}
