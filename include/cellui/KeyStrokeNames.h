#pragma once

#include <cellui/KeyStroke.h>
#include <cellui/KeymapProfile.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cellui {
namespace KeyStroke {

// A key sequence as a person reads it: "C-x C-s", "M-w", "C-S-Up" on the
// terminal keymap, "Ctrl+S", "Ctrl+Shift+Z" on the desktop one.
std::string describe(const std::vector<uint64_t> & sequence, KeymapProfile profile);

// The inverse of describe(), in either spelling: "q", "C-x C-c", "M-x", "C-S-Up", "RET" (Emacs's, where M- is
// Alt), or "Ctrl+Q", "Ctrl+Shift+P", "Alt+Enter" (the desktop's, where a letter is the key whatever its case).
// Key names are describe()'s in both forms (Up, PageDown, F5, DEL/Backspace, RET/Enter, TAB/Tab, ESC/Esc,
// SPC/Space); anything else is one character. Empty when `keys` is not a key sequence.
std::vector<uint64_t> parse(std::string_view keys);

}  // namespace KeyStroke
}  // namespace cellui
