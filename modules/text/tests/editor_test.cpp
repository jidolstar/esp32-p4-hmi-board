#include <cassert>
#include <string>

#include "hmi/text/editor.hpp"

namespace {

using hmi::text::EditStatus;
using hmi::text::Editor;

void verify_utf8_boundaries_and_middle_edit() {
  Editor editor;
  assert(editor.set_text("A한😀") == EditStatus::ok);
  assert(editor.byte_size() == 8U);
  assert(editor.codepoint_count() == 3U);
  assert(editor.cursor_codepoint_index() == 3U);

  assert(editor.move_cursor(-2) == EditStatus::ok);
  assert(editor.cursor_codepoint_index() == 1U);
  assert(editor.insert("중") == EditStatus::ok);
  assert(editor.text() == "A중한😀");
  assert(editor.backspace() == EditStatus::ok);
  assert(editor.text() == "A한😀");
  assert(editor.delete_forward() == EditStatus::ok);
  assert(editor.text() == "A😀");
}

void verify_invalid_and_capacity_failures_preserve_text() {
  Editor editor;
  assert(editor.set_text("safe") == EditStatus::ok);
  const auto before = std::string(editor.text());
  const auto cursor_before = editor.cursor_byte_offset();

  assert(editor.insert("\xF0\x9F\x98") == EditStatus::invalid_utf8);
  assert(editor.text() == before);
  assert(editor.cursor_byte_offset() == cursor_before);

  assert(editor.set_text(std::string(Editor::kMaxCodepoints + 1U, 'a')) ==
         EditStatus::codepoint_limit);
  assert(editor.text() == before);
  assert(editor.set_text(std::string(Editor::kMaxBytes + 1U, 'a')) ==
         EditStatus::byte_limit);
  assert(editor.text() == before);
}

void verify_four_byte_delete_and_clear() {
  Editor editor;
  assert(editor.set_text("😀B") == EditStatus::ok);
  editor.move_to_start();
  assert(editor.delete_forward() == EditStatus::ok);
  assert(editor.text() == "B");
  editor.clear();
  assert(editor.text().empty());
  assert(editor.cursor_byte_offset() == 0U);
}

void verify_absolute_cursor_movement() {
  Editor editor;
  assert(editor.set_text("A한😀B") == EditStatus::ok);

  assert(editor.move_cursor_to(0U) == EditStatus::ok);
  assert(editor.cursor_byte_offset() == 0U);
  assert(editor.move_cursor_to(2U) == EditStatus::ok);
  assert(editor.cursor_codepoint_index() == 2U);
  assert(editor.cursor_byte_offset() == 4U);
  assert(editor.move_cursor_to(4U) == EditStatus::ok);
  assert(editor.cursor_byte_offset() == editor.byte_size());

  const auto cursor_before = editor.cursor_byte_offset();
  assert(editor.move_cursor_to(5U) == EditStatus::cursor_out_of_range);
  assert(editor.cursor_byte_offset() == cursor_before);
}

}  // namespace

int main() {
  verify_utf8_boundaries_and_middle_edit();
  verify_invalid_and_capacity_failures_preserve_text();
  verify_four_byte_delete_and_clear();
  verify_absolute_cursor_movement();
  return 0;
}
