#include <cassert>
#include <string>

#include "hmi/text/input_model.hpp"

namespace {

std::size_t find_key(const hmi::text::InputModel& model, std::uint32_t codepoint) {
  const auto layout = hmi::text::keyboard_layout(model.mode(), model.shift());
  for (std::size_t index = 0U; index < layout.size; ++index) {
    if (layout.keys[index].codepoint == codepoint) {
      return index;
    }
  }
  return layout.size;
}

void press(hmi::text::InputModel& model, std::uint32_t codepoint) {
  const auto index = find_key(model, codepoint);
  assert(index < hmi::text::keyboard_layout(model.mode(), model.shift()).size);
  assert(model.press_key(index) == hmi::text::EditStatus::ok);
}

}  // namespace

int main() {
  using hmi::text::EditStatus;
  using hmi::text::InputModel;
  using hmi::text::KeyboardMode;

  {
    InputModel ime;
    assert(ime.insert_codepoint(0xD55CU) == EditStatus::ok);
    assert(ime.insert_codepoint('A') == EditStatus::ok);
    assert(ime.editor().text() == "한A");
  }

  InputModel model;
  press(model, 0x314EU);  // ㅎ
  press(model, 0x314FU);  // ㅏ
  press(model, 0x3134U);  // ㄴ
  assert(model.is_composing());

  // mode 전환은 조합 중인 '한'을 먼저 확정하고 SHIFT를 초기화한다.
  assert(model.set_mode(KeyboardMode::latin) == EditStatus::ok);
  assert(model.editor().text() == "한");
  model.toggle_shift();
  press(model, 'A');
  assert(model.editor().text() == "한A");

  // cursor 이동도 조합을 확정한 뒤 UTF-8 codepoint 경계로 이동한다.
  assert(model.set_mode(KeyboardMode::hangul) == EditStatus::ok);
  press(model, 0x3131U);  // ㄱ
  press(model, 0x314FU);  // ㅏ
  assert(model.move_cursor(-1) == EditStatus::ok);
  assert(!model.is_composing());
  assert(model.editor().text() == "한A가");
  assert(model.editor().cursor_codepoint_index() == 2U);

  assert(model.backspace() == EditStatus::ok);
  assert(model.editor().text() == "한가");

  // 절대 위치 이동은 조합 preview를 display codepoint 하나로 계산한 뒤 확정한다.
  model.clear();
  assert(model.set_text("AB") == EditStatus::ok);
  assert(model.move_cursor_to(1U) == EditStatus::ok);
  press(model, 0x314EU);  // ㅎ
  press(model, 0x314FU);  // ㅏ
  press(model, 0x3134U);  // ㄴ
  assert(model.is_composing());
  assert(model.move_cursor_to(2U) == EditStatus::ok);
  assert(!model.is_composing());
  assert(model.editor().text() == "A한B");
  assert(model.editor().cursor_codepoint_index() == 2U);

  // 범위 밖 요청은 조합과 editor cursor를 모두 보존한다.
  press(model, 0x3131U);  // ㄱ
  const auto cursor_before = model.editor().cursor_byte_offset();
  assert(model.move_cursor_to(5U) == EditStatus::cursor_out_of_range);
  assert(model.is_composing());
  assert(model.editor().cursor_byte_offset() == cursor_before);
  assert(model.preview_codepoint() == 0x3131U);
  assert(model.backspace() == EditStatus::ok);

  // Space는 현재 UTF-8 cursor에 삽입되고 앞뒤 문자열을 보존한다.
  assert(model.insert_space() == EditStatus::ok);
  assert(model.editor().text() == "A한 B");
  assert(model.editor().cursor_codepoint_index() == 3U);

  // 조합 preview와 Space를 모두 담을 수 없으면 원본과 조합 상태를 보존한다.
  model.clear();
  assert(model.set_text(std::string(hmi::text::Editor::kMaxCodepoints - 1U, 'a')) ==
         EditStatus::ok);
  press(model, 0x314EU);  // ㅎ
  assert(model.insert_space() == EditStatus::codepoint_limit);
  assert(model.is_composing());
  assert(model.editor().codepoint_count() == hmi::text::Editor::kMaxCodepoints - 1U);

  model.clear();
  assert(model.editor().text().empty());
  return 0;
}
