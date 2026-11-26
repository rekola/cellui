#include <cellui/TextField.h>

#include <cellui/Plane.h>
#include <cellui/Utf8.h>

namespace cellui {

void TextField::paste(const std::string & text) {
  std::string line;
  line.reserve(text.size());
  for (size_t i = 0; i < text.size(); i++) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;  // CR LF is one break
    line += (c < 0x20 || c == 0x7f) ? ' ' : static_cast<char>(c);
  }
  editor_.insert(line);
}

TextField::Drawn TextField::draw(Plane & plane, int row, int col, int width) {
  LineEditor::View v = editor_.view(width);
  plane.putstr(row, col, v.text);
  return Drawn{std::move(v.text), v.cursor_column};
}

std::string TextField::cellUnder(const Drawn & drawn) {
  int at = 0;
  for (const std::string & cluster : utf8::clusters(drawn.text)) {
    if (at == drawn.cursor_column) return cluster;
    at += utf8::displayWidth(cluster);
  }
  return " ";
}

}  // namespace cellui
