#pragma once

#include <cellui/CommandRegistry.h>
#include <cellui/KeyStrokeNames.h>
#include <cellui/KeymapProfile.h>

#include <cassert>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cellui {

// Maps key shortcuts (see KeyStroke.h) to command names, not directly to
// callables - this indirection is what lets a bound command also be invoked
// by name (e.g. from an command palette), mirroring how terminal keymaps
// bind keys to command symbols rather than to functions directly.
//
// Also supports two-key Emacs-style prefix sequences (C-x C-s, C-x C-c, ...)
// via bindPrefixed()/isPrefix()/lookupPrefixed() - a second, nested table
// rather than folding the prefix into the plain shortcut space, since a prefix
// shortcut (C-x) and a plain binding can coexist independently (nothing here
// stops some other widget from binding plain C-x to something outright) and
// because UIElement::dispatchCommand() needs to distinguish "this shortcut
// starts a sequence" from "this shortcut is itself a complete command" before
// it knows whether to wait for a second key at all.
class Keymap {
 public:
  // A binding made without a profile works in every profile; one made for a
  // profile only there, and wins over a shared binding of the same shortcut.
  void bind(uint64_t stroke, std::string command_name) {
    shared_.bindings[stroke] = std::move(command_name);
  }
  void bind(KeymapProfile profile, uint64_t stroke, std::string command_name) {
    table(profile).bindings[stroke] = std::move(command_name);
  }

  // The same with the keys as a person writes them (KeyStroke::parse()): "q", "C-x C-c", "Ctrl+Shift+P". One key
  // or a prefix and a key; false (and an assertion) for anything else.
  bool bind(std::string_view keys, std::string command_name) { return bindKeys(shared_, keys, std::move(command_name)); }
  bool bind(KeymapProfile profile, std::string_view keys, std::string command_name) {
    return bindKeys(table(profile), keys, std::move(command_name));
  }

  const std::string * lookup(uint64_t stroke, KeymapProfile profile) const {
    if (auto name = find(table(profile).bindings, stroke)) return name;
    return find(shared_.bindings, stroke);
  }

  void bindPrefixed(uint64_t prefix, uint64_t stroke, std::string command_name) {
    shared_.prefixed[prefix][stroke] = std::move(command_name);
  }
  void bindPrefixed(KeymapProfile profile, uint64_t prefix, uint64_t stroke, std::string command_name) {
    table(profile).prefixed[prefix][stroke] = std::move(command_name);
  }

  bool isPrefix(uint64_t stroke, KeymapProfile profile) const {
    return table(profile).prefixed.count(stroke) > 0 || shared_.prefixed.count(stroke) > 0;
  }

  // `prefix` (e.g. C-x) then `shortcut` (e.g. C-s), as dispatchCommand() tracks them.
  const std::string * lookupPrefixed(uint64_t prefix, uint64_t stroke, KeymapProfile profile) const {
    if (auto name = findPrefixed(table(profile), prefix, stroke)) return name;
    return findPrefixed(shared_, prefix, stroke);
  }

  // Every key sequence (one shortcut, or a prefix and a shortcut) that runs
  // `command_name` in `profile`, in no particular order.
  std::vector<std::vector<uint64_t>> sequencesFor(const std::string & command_name, KeymapProfile profile) const {
    std::vector<std::vector<uint64_t>> result;
    for (const Table * t : {&shared_, &table(profile)}) {
      for (auto & [stroke, name] : t->bindings) {
	if (name == command_name && lookup(stroke, profile) == &name) result.push_back({stroke});
      }
      for (auto & [prefix, strokes] : t->prefixed) {
	for (auto & [stroke, name] : strokes) {
	  if (name == command_name && lookupPrefixed(prefix, stroke, profile) == &name) result.push_back({prefix, stroke});
	}
      }
    }
    return result;
  }

  // How `command_name` is bound in `profile` as a person reads it ("C-x C-s", "Ctrl+S"): the simplest of its
  // bindings (fewest keys, then fewest modifiers); "" if it has none. A raw control byte (a terminal's own spelling
  // of a shortcut that is bound as well) is never the one shown.
  std::string bindingFor(const std::string & command_name, KeymapProfile profile) const {
    auto modifiers = [](const std::vector<uint64_t> & sequence) {
      int n = 0;
      for (uint64_t stroke : sequence) n += KeyStroke::hasCtrl(stroke) + KeyStroke::hasAlt(stroke) + KeyStroke::hasShift(stroke) + KeyStroke::hasMeta(stroke);
      return n;
    };
    auto simpler = [&](const std::vector<uint64_t> & a, const std::vector<uint64_t> & b) {
      if (a.size() != b.size()) return a.size() < b.size();
      if (modifiers(a) != modifiers(b)) return modifiers(a) < modifiers(b);
      return a < b;
    };
    std::vector<uint64_t> best;
    for (auto & sequence : sequencesFor(command_name, profile)) {
      bool printable = true;
      for (uint64_t stroke : sequence) {
        int id = KeyStroke::idOf(stroke);
        if (id < 0x20 && id != Key::Tab && id != Key::Escape) printable = false;
      }
      if (printable && (best.empty() || simpler(sequence, best))) best = sequence;
    }
    return best.empty() ? std::string() : KeyStroke::describe(best, profile);
  }

  // Startup-time safety net: a typo'd command name is an assertion failure
  // instead of a silently dead keybinding.
  bool allBoundIn(const CommandRegistry & registry) const {
    return allBound(shared_, registry) && allBound(emacs_, registry) && allBound(desktop_, registry);
  }

 private:
  using Strokes = std::unordered_map<uint64_t, std::string>;
  struct Table {
    Strokes bindings;
    std::unordered_map<uint64_t, Strokes> prefixed;
  };

  Table & table(KeymapProfile profile) { return profile == KeymapProfile::TERMINAL ? emacs_ : desktop_; }
  const Table & table(KeymapProfile profile) const { return profile == KeymapProfile::TERMINAL ? emacs_ : desktop_; }

  static bool bindKeys(Table & t, std::string_view keys, std::string command_name) {
    std::vector<uint64_t> sequence = KeyStroke::parse(keys);
    assert((sequence.size() == 1 || sequence.size() == 2) && "Keymap::bind: not one key or a prefix and a key");
    if (sequence.size() == 1) t.bindings[sequence[0]] = std::move(command_name);
    else if (sequence.size() == 2) t.prefixed[sequence[0]][sequence[1]] = std::move(command_name);
    else return false;
    return true;
  }

  static const std::string * find(const Strokes & strokes, uint64_t stroke) {
    auto it = strokes.find(stroke);
    return it != strokes.end() ? &it->second : nullptr;
  }
  static const std::string * findPrefixed(const Table & t, uint64_t prefix, uint64_t stroke) {
    auto it = t.prefixed.find(prefix);
    return it == t.prefixed.end() ? nullptr : find(it->second, stroke);
  }
  static bool allBound(const Table & t, const CommandRegistry & registry) {
    for (auto & [stroke, name] : t.bindings) {
      if (!registry.has(name)) return false;
    }
    for (auto & [prefix, strokes] : t.prefixed) {
      for (auto & [stroke, name] : strokes) {
	if (!registry.has(name)) return false;
      }
    }
    return true;
  }

  Table shared_, emacs_, desktop_;
};

}  // namespace cellui
