#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hmi::text {

enum class KeyboardMode : std::uint8_t {
  hangul,
  latin,
  number,
  symbol,
};

struct KeyboardKey {
  std::string_view label;
  std::uint32_t codepoint = 0U;
};

/**
 * 문자 key의 한 행을 설명한다. 너비 단위는 반 key 단위라서 1은 0.5 key 들여쓰기다.
 */
struct KeyboardRowDescriptor {
  std::size_t key_offset = 0U;
  std::size_t key_count = 0U;
  std::uint8_t leading_half_units = 0U;
  std::uint8_t key_width_half_units = 2U;
};

struct KeyboardLayoutView {
  const KeyboardKey* keys = nullptr;
  std::size_t size = 0U;
  const KeyboardRowDescriptor* rows = nullptr;
  std::size_t row_count = 0U;
};

/** keyboard mode와 SHIFT에 해당하는 immutable 문자 key 배열을 반환한다. */
KeyboardLayoutView keyboard_layout(KeyboardMode mode, bool shift) noexcept;

}  // namespace hmi::text
