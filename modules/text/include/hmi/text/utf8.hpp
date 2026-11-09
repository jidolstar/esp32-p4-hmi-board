#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hmi::text::utf8 {

/** UTF-8 한 codepoint의 해석 결과다. */
enum class DecodeStatus : std::uint8_t {
  ok,
  end,
  invalid,
};

struct DecodeResult {
  DecodeStatus status = DecodeStatus::end;
  std::uint32_t codepoint = 0;
  std::size_t next_offset = 0;
};

struct ValidationResult {
  DecodeStatus status = DecodeStatus::ok;
  std::size_t bytes = 0U;
  std::size_t codepoints = 0U;
  std::size_t error_offset = 0U;
};

struct EncodedCodepoint {
  std::array<char, 4U> bytes{};
  std::size_t size = 0U;
};

/**
 * 길이가 정해진 UTF-8 문자열에서 한 codepoint를 안전하게 읽는다.
 *
 * 정상 입력은 다음 문자의 byte offset을 반환한다. 잘못된 입력은 호출자가 반복을
 * 계속할 수 있도록 현재 byte 하나를 건너뛴 offset과 `invalid`를 반환한다.
 * offset이 문자열 끝 이상이면 `end`를 반환하며 입력 범위 밖을 읽지 않는다.
 */
DecodeResult decode_next(std::string_view text, std::size_t offset) noexcept;

/** 전체 문자열의 UTF-8 유효성과 byte/codepoint 수를 한 번에 검사한다. */
ValidationResult validate(std::string_view text) noexcept;

/** Unicode scalar value 하나를 UTF-8로 변환한다. 잘못된 값은 size 0을 반환한다. */
EncodedCodepoint encode(std::uint32_t codepoint) noexcept;

}  // namespace hmi::text::utf8
