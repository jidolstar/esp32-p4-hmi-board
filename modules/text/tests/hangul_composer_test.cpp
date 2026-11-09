#include <cassert>
#include <string>

#include "hmi/text/hangul_composer.hpp"

namespace {

using hmi::text::EditStatus;
using hmi::text::Editor;
using hmi::text::HangulComposer;

void input(HangulComposer& composer, Editor& editor,
           std::initializer_list<std::uint32_t> jamo) {
  for (const auto codepoint : jamo) {
    assert(composer.input_jamo(editor, codepoint) == EditStatus::ok);
  }
}

void verify_basic_and_compound_composition() {
  Editor editor;
  HangulComposer composer;
  input(composer, editor, {0x314EU, 0x314FU, 0x3134U});  // ㅎㅏㄴ
  assert(composer.preview_codepoint() == 0xD55CU);
  assert(composer.commit(editor) == EditStatus::ok);
  assert(editor.text() == "한");

  editor.clear();
  input(composer, editor, {0x3131U, 0x3157U, 0x314FU});  // ㄱㅗㅏ
  assert(composer.commit(editor) == EditStatus::ok);
  assert(editor.text() == "과");

  editor.clear();
  input(composer, editor, {0x3131U, 0x314FU, 0x3142U, 0x3145U});  // ㄱㅏㅂㅅ
  assert(composer.commit(editor) == EditStatus::ok);
  assert(editor.text() == "값");
}

void verify_trailing_split_and_backspace() {
  Editor editor;
  HangulComposer composer;
  input(composer, editor, {0x3131U, 0x314FU, 0x3142U, 0x3145U, 0x3163U});
  assert(composer.commit(editor) == EditStatus::ok);
  assert(editor.text() == "갑시");

  editor.clear();
  input(composer, editor, {0x3131U, 0x314FU, 0x3142U, 0x3145U});
  assert(composer.backspace(editor) == EditStatus::ok);
  assert(composer.preview_codepoint() == 0xAC11U);  // 갑
  assert(composer.backspace(editor) == EditStatus::ok);
  assert(composer.preview_codepoint() == 0xAC00U);  // 가
  assert(composer.backspace(editor) == EditStatus::ok);
  assert(composer.preview_codepoint() == 0x3131U);  // ㄱ
  assert(composer.backspace(editor) == EditStatus::ok);
  assert(!composer.is_composing());
}

void verify_middle_insert_and_capacity_preservation() {
  Editor editor;
  HangulComposer composer;
  assert(editor.set_text("AB") == EditStatus::ok);
  assert(editor.move_cursor(-1) == EditStatus::ok);
  input(composer, editor, {0x314EU, 0x314FU, 0x3134U});
  assert(composer.commit(editor) == EditStatus::ok);
  assert(editor.text() == "A한B");

  assert(editor.set_text(std::string(Editor::kMaxCodepoints, 'x')) == EditStatus::ok);
  input(composer, editor, {0x3131U, 0x314FU});
  const auto before = std::string(editor.text());
  assert(composer.commit(editor) == EditStatus::codepoint_limit);
  assert(editor.text() == before);
  assert(composer.is_composing());
  composer.cancel();
}

}  // namespace

int main() {
  verify_basic_and_compound_composition();
  verify_trailing_split_and_backspace();
  verify_middle_insert_and_capacity_preservation();
  return 0;
}
