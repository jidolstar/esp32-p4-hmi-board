#pragma once

#include <cstdint>

#include "hmi/text/editor.hpp"

namespace hmi::text {

/**
 * 현대 한글 초성·중성·종성을 조합해 Editor에 확정하는 두벌식 조합기다.
 *
 * 조합 중에는 최대 한 codepoint만 별도 상태로 보유한다. commit 실패 시 조합 상태와
 * Editor를 유지하므로 page가 길이 오류를 표시한 뒤 사용자가 삭제할 수 있다.
 */
class HangulComposer {
 public:
  [[nodiscard]] bool is_composing() const noexcept;
  [[nodiscard]] std::uint32_t preview_codepoint() const noexcept;

  EditStatus input_jamo(Editor& editor, std::uint32_t compatibility_jamo) noexcept;
  EditStatus commit(Editor& editor) noexcept;
  EditStatus backspace(Editor& editor) noexcept;
  void cancel() noexcept;

 private:
  std::int8_t leading_ = -1;
  std::int8_t vowel_ = -1;
  std::int8_t trailing_ = 0;
};

static_assert(sizeof(HangulComposer) <= 4U,
              "HangulComposer must remain a small bounded state machine");

}  // namespace hmi::text
