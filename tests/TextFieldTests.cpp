#include "TestFramework.h"

#include <cellui/TextField.h>

using namespace cellui;

TEST(text_field_paste_is_one_line_inserted_at_the_cursor) {
  TextField f;
  f.setText("ac");
  f.key(KeyDown{Key::Left, {}, "", false, false, {}});
  f.paste("b");
  CHECK(f.text() == "abc");
  f.paste("1\r\n2\n3\t4");
  CHECK(f.text() == "ab1 2 3 4c");
}

TEST(text_field_cell_under_the_cursor_is_a_space_past_the_end) {
  TextField::Drawn d{"ab", 2};
  CHECK(TextField::cellUnder(d) == " ");
  d.cursor_column = 1;
  CHECK(TextField::cellUnder(d) == "b");
}
