#include <cellui/KeyStrokeNames.h>

#include <cellui/Utf8.h>

#include <cctype>

namespace cellui {
namespace KeyStroke {

namespace {

std::string keyName(int id, bool emacs, bool upper) {
  switch (id) {
    case Key::Up: return "Up";
    case Key::Down: return "Down";
    case Key::Left: return "Left";
    case Key::Right: return "Right";
    case Key::Insert: return "Insert";
    case Key::Delete: return "Delete";
    case Key::Backspace: return emacs ? "DEL" : "Backspace";
    case Key::PageUp: return "PageUp";
    case Key::PageDown: return "PageDown";
    case Key::Home: return "Home";
    case Key::End: return "End";
    case Key::Enter: return emacs ? "RET" : "Enter";
    case Key::Tab: return emacs ? "TAB" : "Tab";
    case Key::Escape: return emacs ? "ESC" : "Esc";
    case Key::Space: return emacs ? "SPC" : "Space";
  }
  if (id >= Key::F1 && id <= Key::F1 + 11) return "F" + std::to_string(id - Key::F1 + 1);
  if (id >= 0x20 && id < Key::First) {
    if (id < 0x80 && std::isalpha(id) && upper) return std::string(1, static_cast<char>(std::toupper(id)));
    return utf8::encodeCodepoint(static_cast<uint32_t>(id));
  }
  return "?";
}

}  // namespace

std::string describe(const std::vector<uint64_t> & sequence, KeymapProfile profile) {
  bool emacs = profile == KeymapProfile::TERMINAL;
  std::string out;
  for (uint64_t stroke : sequence) {
    if (!out.empty()) out += ' ';
    int id = idOf(stroke);
    bool letter = id < 0x80 && std::isalpha(id);
    if (emacs) {
      if (hasCtrl(stroke)) out += "C-";
      if (hasAlt(stroke) || hasMeta(stroke)) out += "M-";
      if (hasShift(stroke)) out += "S-";
      out += keyName(id, true, hasShift(stroke) && letter);
    } else {
      if (hasCtrl(stroke)) out += "Ctrl+";
      if (hasAlt(stroke)) out += "Alt+";
      if (hasShift(stroke)) out += "Shift+";
      if (hasMeta(stroke)) out += "Meta+";
      out += keyName(id, false, true);
    }
  }
  return out;
}

namespace {

bool sameText(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

// A key's id from its name or its one character; -1 if it is neither.
int keyId(std::string_view name) {
  static const struct {
    const char * name;
    int id;
  } kNames[] = {{"Up", Key::Up},           {"Down", Key::Down},     {"Left", Key::Left},          {"Right", Key::Right},
                {"Insert", Key::Insert},   {"Delete", Key::Delete}, {"DEL", Key::Backspace},      {"Backspace", Key::Backspace},
                {"PageUp", Key::PageUp},   {"PageDown", Key::PageDown}, {"Home", Key::Home},      {"End", Key::End},
                {"RET", Key::Enter},       {"Enter", Key::Enter},   {"TAB", Key::Tab},            {"ESC", Key::Escape},
                {"Esc", Key::Escape},      {"SPC", Key::Space},     {"Space", Key::Space}};
  if (name.size() > 1) {
    for (auto & key : kNames) {
      if (sameText(name, key.name)) return key.id;
    }
    if ((name[0] == 'F' || name[0] == 'f') && name.size() <= 3) {
      int n = 0;
      for (char c : name.substr(1)) {
        if (c < '0' || c > '9') return -1;
        n = n * 10 + (c - '0');
      }
      if (n >= 1 && n <= 12) return Key::F1 + n - 1;
    }
  }
  // One UTF-8 character.
  if (name.empty()) return -1;
  auto lead = static_cast<unsigned char>(name[0]);
  size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
  if (length == 0 || name.size() != length) return -1;
  int cp = length == 1 ? lead : lead & (0x7F >> length);
  for (size_t i = 1; i < length; i++) {
    auto c = static_cast<unsigned char>(name[i]);
    if ((c & 0xC0) != 0x80) return -1;
    cp = (cp << 6) | (c & 0x3F);
  }
  return cp;
}

uint64_t parseStroke(std::string_view s) {
  bool ctrl = false, alt = false, shift = false, meta = false;
  bool desktop = s.size() > 1 && s.find('+', 0) != std::string_view::npos && s.find('+') < s.size() - 1;
  if (desktop) {
    // Ctrl+Shift+P: modifiers joined by '+', the key last (which may itself be '+', as in Ctrl++).
    while (true) {
      size_t plus = s.find('+', 1);
      if (plus == std::string_view::npos) break;
      std::string_view mod = s.substr(0, plus);
      if (sameText(mod, "Ctrl")) ctrl = true;
      else if (sameText(mod, "Alt")) alt = true;
      else if (sameText(mod, "Shift")) shift = true;
      else if (sameText(mod, "Meta")) meta = true;
      else return 0;
      s.remove_prefix(plus + 1);
    }
  } else {
    // C-M-S-x: one-letter modifiers each followed by '-', the key last (which may itself be '-').
    while (s.size() > 2 && s[1] == '-') {
      if (s[0] == 'C') ctrl = true;
      else if (s[0] == 'M') alt = true;
      else if (s[0] == 'S') shift = true;
      else return 0;
      s.remove_prefix(2);
    }
  }
  int id = keyId(s);
  if (id < 0) return 0;
  // A letter named with a modifier is the key, Shift or not: Ctrl+Q is q with Ctrl, C-S-p is p with Ctrl and Shift.
  if (id < 0x80 && std::isalpha(id) && (desktop || ctrl || alt || shift || meta)) id = std::tolower(id);
  return pack(id, ctrl, alt, shift, meta);
}

}  // namespace

std::vector<uint64_t> parse(std::string_view keys) {
  std::vector<uint64_t> sequence;
  size_t at = 0;
  while (at < keys.size()) {
    if (keys[at] == ' ') {
      at++;
      continue;
    }
    size_t end = keys.find(' ', at);
    if (end == std::string_view::npos) end = keys.size();
    uint64_t stroke = parseStroke(keys.substr(at, end - at));
    if (stroke == 0) return {};
    sequence.push_back(stroke);
    at = end;
  }
  return sequence;
}

}  // namespace KeyStroke
}  // namespace cellui
