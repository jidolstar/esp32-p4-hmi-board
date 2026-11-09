#pragma once

#include <cstddef>
#include <string_view>

#include "hmi/text/editor.hpp"
#include "hmi/text/hangul_composer.hpp"
#include "hmi/text/keyboard_layout.hpp"

namespace hmi::text {

/**
 * keyboard mode, SHIFT, UTF-8 editor와 한글 조합 수명을 한곳에서 관리한다.
 *
 * cursor 이동·mode 전환·완료 전에 조합을 확정한다. 확정이 용량 제한으로 실패하면
 * mode와 cursor를 바꾸지 않아 page가 기존 상태를 그대로 다시 표시할 수 있다.
 */
class InputModel {
 public:
  [[nodiscard]] const Editor& editor() const noexcept { return editor_; }
  [[nodiscard]] KeyboardMode mode() const noexcept { return mode_; }
  [[nodiscard]] bool shift() const noexcept { return shift_; }
  [[nodiscard]] bool is_composing() const noexcept { return composer_.is_composing(); }
  [[nodiscard]] std::uint32_t preview_codepoint() const noexcept {
    return composer_.preview_codepoint();
  }

  EditStatus set_text(std::string_view text) noexcept;
  EditStatus press_key(std::size_t key_index) noexcept;
  /** Windows IME처럼 이미 조합된 Unicode scalar를 현재 cursor에 삽입한다. */
  EditStatus insert_codepoint(std::uint32_t codepoint) noexcept;
  EditStatus set_mode(KeyboardMode mode) noexcept;
  void toggle_shift() noexcept { shift_ = !shift_; }
  EditStatus move_cursor(int codepoint_delta) noexcept;
  /** 표시 중인 조합 글자를 포함한 codepoint index로 cursor를 옮긴다. */
  EditStatus move_cursor_to(std::size_t display_codepoint_index) noexcept;
  /** 현재 cursor에 ASCII Space를 삽입하며 진행 중인 한글 조합을 먼저 확정한다. */
  EditStatus insert_space() noexcept;
  EditStatus backspace() noexcept;
  EditStatus delete_forward() noexcept;
  EditStatus complete() noexcept;
  void clear() noexcept;

 private:
  Editor editor_{};
  HangulComposer composer_{};
  KeyboardMode mode_ = KeyboardMode::hangul;
  bool shift_ = false;
};

static_assert(sizeof(InputModel) <= 232U,
              "InputModel must remain bounded for persistent page state");

}  // namespace hmi::text
