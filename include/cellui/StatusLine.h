#pragma once

#include <cellui/InputEvents.h>
#include <cellui/Widget.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace cellui {

// The bottom row of a screen: a one-line message, and the prompt that takes its
// place while a command or a name is typed (Emacs's echo area and minibuffer).
// It has no background of its own, because a display cannot draw the last cell
// of the last row, so a coloured row would end in a different one.
//
// The prompt is Plane::showReader() on the line's own plane; a message set
// while it is open waits until it closes. Enter submits what is typed, Ctrl-G
// cancels without running anything, and Tab completes (a set of names, or file
// paths) with the indicators "[No match]" and "[Sole completion]" after the
// typed text. The owner gives it the keys (keyDown(), keyUp()) before its own.
class StatusLine : public Widget {
 public:
  explicit StatusLine(Plane & parent) : Widget(parent) {
    getPlane().makeTransparent();
    getPlane().setBgDefault();
  }

  // The shortcut that opens the command prompt: Meta-x on the terminal keymap
  // (a terminal's ESC then x is folded into one Meta event by KeyReader),
  // Ctrl+Shift+P on the desktop one.
  bool isCommandPromptShortcut(const KeyDown & e) const {
    if (keymapProfile() == KeymapProfile::DESKTOP) return e.mods.ctrl && e.mods.shift && !e.mods.alt && !e.mods.meta && e.key == 'p';
    return (e.mods.alt || e.mods.meta) && (e.key == 'x' || e.key == 'X');
  }

  bool isReaderActive() const { return getPlane().readerActive(); }

  void setMessage(std::string s) {
    if (getPlane().readerActive()) {
      pending_message_ = std::move(s);
    } else {
      erase();
      putstr(0, 0, s);
    }
  }

  // Opens the prompt with `prompt`; `on_submit` gets the text on Enter. Opening
  // it first runs the before-show callback, so the owner can cancel any other
  // half-typed edit: entering the prompt aborts what else had the keyboard.
  void showPrompt(const std::string & prompt, std::function<void(const std::string &)> on_submit, const std::string & initial_text = "") {
    if (before_show_prompt_) before_show_prompt_();
    on_submit_ = std::move(on_submit);
    getPlane().showReader(prompt, 0, -1, -1, -1, initial_text);
  }

  // The prompt with Tab completion against `candidates_for(text)`: every full
  // answer that starts with what is typed (command names, buffer names). Enter
  // still submits whatever is typed, matched or not.
  void showPromptWithCompletion(const std::string & prompt, std::function<void(const std::string &)> on_submit,
                                std::function<std::set<std::string>(const std::string &)> candidates_for, const std::string & initial_text = "") {
    tab_completer_ = [this, candidates_for]() { completeAgainstSet(candidates_for); };
    indicator_shown_ = false;
    showPrompt(prompt, std::move(on_submit), initial_text);
  }

  // The prompt with Tab completion of file paths, one path component at a time.
  void showFilePrompt(const std::string & prompt, std::function<void(const std::string &)> on_submit, const std::string & initial_text = "") {
    tab_completer_ = [this]() { completeFilePath(); };
    indicator_shown_ = false;
    showPrompt(prompt, std::move(on_submit), initial_text);
  }

  // What the command prompt shortcut does (open a command palette).
  void setCommandPaletteCallback(std::function<void()> cb) { open_command_palette_ = std::move(cb); }
  void setBeforeShowPromptCallback(std::function<void()> cb) { before_show_prompt_ = std::move(cb); }

  // A key's release never submits or edits the prompt.
  bool keyUp(const KeyUp &) { return getPlane().readerActive(); }

  // Whether the line took the key: all of them while the prompt is open, else
  // only the command prompt shortcut.
  bool keyDown(const KeyDown & e) {
    if (getPlane().readerActive()) {
      if (tab_completer_ && e.key == Key::Tab) {
        // A copy: finishing the prompt reassigns tab_completer_ under the call.
        auto completer = tab_completer_;
        completer();
      } else if (e.key == Key::Enter) {
        submitReader();
      } else if (e.mods.ctrl && e.key == 'g') {
        cancelReader();
      } else {
        // Typing on makes an indicator from an earlier Tab stale.
        showIndicator("");
        return getPlane().offerKey(e);
      }
      return true;
    }
    if (isCommandPromptShortcut(e)) {
      if (open_command_palette_) open_command_palette_();
      return true;
    }
    return false;
  }

  std::string closeReader() {
    tab_completer_ = nullptr;
    auto text = getPlane().closeReader();
    if (pending_message_.empty()) {
      setMessage("");
    } else {
      setMessage(std::move(pending_message_));
      pending_message_.clear();
    }
    return text;
  }

  // Aborts the prompt without running on_submit: Ctrl-G, and a command reached
  // another way (a menu click) while a prompt is open.
  void cancelReader() {
    on_submit_ = nullptr;
    closeReader();
  }

 private:
  // Extends the typed text as far as every candidate with that prefix agrees,
  // or flashes "[No match]". A press that makes progress completes silently;
  // "[Sole completion]" shows on a second press, once the text is the one
  // candidate. It never submits by itself.
  void completeAgainstSet(const std::function<std::set<std::string>(const std::string &)> & candidates_for) {
    auto text = getPlane().getReaderContents();
    auto matches = candidates_for(text);
    if (matches.empty()) {
      showIndicator("[No match]");
      return;
    }
    auto completed = longestCommonPrefix(matches);
    if (completed.size() > text.size()) {
      getPlane().setReaderContents(completed);
      showIndicator("");
    } else {
      showIndicator(matches.size() == 1 ? "[Sole completion]" : "");
    }
  }

  // Completes the last path component against what is in its directory;
  // completing into a directory keeps going. Backup files (a trailing "~") are
  // left out, so "a.xml" is the sole match for "a" next to "a.xml~".
  void completeFilePath() {
    namespace fs = std::filesystem;
    auto text = getPlane().getReaderContents();
    std::string dir_part, partial;
    if (text.empty() || text.back() == '/') {
      dir_part = text;
    } else {
      fs::path typed(text);
      auto parent = typed.parent_path().string();
      dir_part = parent.empty() ? "" : parent + "/";
      partial = typed.filename().string();
    }
    std::error_code ec;
    std::vector<std::string> matches;  // bare entry names; directories carry a trailing "/"
    for (auto & entry : fs::directory_iterator(dir_part.empty() ? "." : dir_part, ec)) {
      if (ec) break;
      auto name = entry.path().filename().string();
      if (name.compare(0, partial.size(), partial) != 0) continue;
      if (!entry.is_directory() && !name.empty() && name.back() == '~') continue;
      matches.push_back(entry.is_directory() ? name + "/" : name);
    }
    if (matches.empty()) {
      showIndicator("[No match]");
      return;
    }
    std::string common = matches.front();
    for (auto & m : matches) {
      size_t i = 0;
      while (i < common.size() && i < m.size() && common[i] == m[i]) i++;
      common.resize(i);
    }
    auto completed = dir_part + common;
    if (completed.size() > text.size()) {
      getPlane().setReaderContents(completed);
      showIndicator("");
    } else {
      showIndicator(matches.size() == 1 ? "[Sole completion]" : "");
    }
  }

  void submitReader() {
    auto text = closeReader();
    auto submit = std::move(on_submit_);
    on_submit_ = nullptr;
    if (submit) submit(text);
  }

  // The names are sorted, so the common prefix of the set is that of its first and last.
  static std::string longestCommonPrefix(const std::set<std::string> & names) {
    const std::string & a = *names.begin();
    const std::string & b = *names.rbegin();
    size_t n = std::min(a.size(), b.size());
    size_t i = 0;
    while (i < n && a[i] == b[i]) i++;
    return a.substr(0, i);
  }

  // Draws or clears ("" clears) the indicator one column past the typed text,
  // on a plane of its own (Plane::showReaderIndicator()). Only while a
  // completing prompt is open; clearing when nothing shows does nothing.
  void showIndicator(const std::string & text) {
    if (!tab_completer_ || (text.empty() && !indicator_shown_)) return;
    if (text.empty()) {
      getPlane().hideReaderIndicator();
    } else {
      auto x = static_cast<int>(getPlane().getReaderContents().size()) + 1;
      getPlane().showReaderIndicator(x, text);
    }
    indicator_shown_ = !text.empty();
  }

  bool indicator_shown_ = false;
  std::string pending_message_;
  std::function<void(const std::string &)> on_submit_;
  std::function<void()> before_show_prompt_;
  std::function<void()> open_command_palette_;
  // Set for the length of a completing prompt; Tab runs it instead of inserting a tab.
  std::function<void()> tab_completer_;
};

}  // namespace cellui
