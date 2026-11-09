#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hmi::text {

enum class EditStatus : std::uint8_t {
  ok,
  invalid_utf8,
  invalid_codepoint,
  byte_limit,
  codepoint_limit,
  cursor_out_of_range,
};

/**
 * 사용자 입력을 UTF-8 경계에서만 수정하는 고정 용량 editor다.
 *
 * 내부 buffer를 직접 노출하지 않으며 삽입 실패 시 text, cursor와 길이를 모두 보존한다.
 * 객체는 page state에 persistent하게 두고 callback stack에 복사하지 않는다.
 */
class Editor {
 public:
  static constexpr std::size_t kMaxBytes = 192U;
  static constexpr std::size_t kMaxCodepoints = 64U;

  [[nodiscard]] std::string_view text() const noexcept {
    return {buffer_.data(), byte_size_};
  }
  [[nodiscard]] std::size_t byte_size() const noexcept { return byte_size_; }
  [[nodiscard]] std::size_t codepoint_count() const noexcept { return codepoint_count_; }
  [[nodiscard]] std::size_t cursor_byte_offset() const noexcept { return cursor_byte_offset_; }
  [[nodiscard]] std::size_t cursor_codepoint_index() const noexcept;

  EditStatus set_text(std::string_view text) noexcept;
  EditStatus insert(std::string_view text) noexcept;
  EditStatus insert_codepoint(std::uint32_t codepoint) noexcept;
  EditStatus backspace() noexcept;
  EditStatus delete_forward() noexcept;
  EditStatus move_cursor(int codepoint_delta) noexcept;
  /** UTF-8 codepoint 경계로만 cursor를 옮기며 범위 밖 요청은 상태를 보존한다. */
  EditStatus move_cursor_to(std::size_t codepoint_index) noexcept;
  void move_to_start() noexcept { cursor_byte_offset_ = 0U; }
  void move_to_end() noexcept { cursor_byte_offset_ = byte_size_; }
  void clear() noexcept;

 private:
  std::size_t previous_offset(std::size_t offset) const noexcept;
  void erase_range(std::size_t begin, std::size_t end, std::size_t removed_codepoints) noexcept;

  std::array<char, kMaxBytes + 1U> buffer_{};
  std::size_t byte_size_ = 0U;
  std::size_t codepoint_count_ = 0U;
  std::size_t cursor_byte_offset_ = 0U;
};

static_assert(sizeof(Editor) <= 224U, "Editor must remain safe for persistent page ownership");

}  // namespace hmi::text
