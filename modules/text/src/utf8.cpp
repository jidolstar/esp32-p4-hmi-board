#include "hmi/text/utf8.hpp"

namespace hmi::text::utf8 {
namespace {

constexpr bool is_continuation(std::uint8_t value) noexcept {
  return (value & 0xC0U) == 0x80U;
}

DecodeResult invalid_at(std::size_t offset) noexcept {
  return {DecodeStatus::invalid, 0U, offset + 1U};
}

}  // namespace

DecodeResult decode_next(std::string_view text, std::size_t offset) noexcept {
  if (offset >= text.size()) {
    return {DecodeStatus::end, 0U, text.size()};
  }

  const auto first = static_cast<std::uint8_t>(text[offset]);
  if (first <= 0x7FU) {
    return {DecodeStatus::ok, first, offset + 1U};
  }

  std::size_t length = 0U;
  std::uint32_t codepoint = 0U;
  if (first >= 0xC2U && first <= 0xDFU) {
    length = 2U;
    codepoint = first & 0x1FU;
  } else if (first >= 0xE0U && first <= 0xEFU) {
    length = 3U;
    codepoint = first & 0x0FU;
  } else if (first >= 0xF0U && first <= 0xF4U) {
    length = 4U;
    codepoint = first & 0x07U;
  } else {
    return invalid_at(offset);
  }

  if (text.size() - offset < length) {
    return invalid_at(offset);
  }

  const auto second = static_cast<std::uint8_t>(text[offset + 1U]);
  if (!is_continuation(second)) {
    return invalid_at(offset);
  }

  // overlong encoding, surrogate, U+10FFFF 초과를 lead/second byte 조합에서 차단한다.
  if ((first == 0xE0U && second < 0xA0U) ||
      (first == 0xEDU && second > 0x9FU) ||
      (first == 0xF0U && second < 0x90U) ||
      (first == 0xF4U && second > 0x8FU)) {
    return invalid_at(offset);
  }

  for (std::size_t index = 1U; index < length; ++index) {
    const auto next = static_cast<std::uint8_t>(text[offset + index]);
    if (!is_continuation(next)) {
      return invalid_at(offset);
    }
    codepoint = (codepoint << 6U) | (next & 0x3FU);
  }

  return {DecodeStatus::ok, codepoint, offset + length};
}

ValidationResult validate(std::string_view text) noexcept {
  std::size_t offset = 0U;
  std::size_t count = 0U;
  while (offset < text.size()) {
    const auto decoded = decode_next(text, offset);
    if (decoded.status != DecodeStatus::ok) {
      return {DecodeStatus::invalid, text.size(), count, offset};
    }
    offset = decoded.next_offset;
    ++count;
  }
  return {DecodeStatus::ok, text.size(), count, text.size()};
}

EncodedCodepoint encode(std::uint32_t codepoint) noexcept {
  EncodedCodepoint result{};
  if (codepoint <= 0x7FU) {
    result.bytes[0] = static_cast<char>(codepoint);
    result.size = 1U;
  } else if (codepoint <= 0x7FFU) {
    result.bytes[0] = static_cast<char>(0xC0U | (codepoint >> 6U));
    result.bytes[1] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    result.size = 2U;
  } else if (codepoint <= 0xFFFFU &&
             !(codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
    result.bytes[0] = static_cast<char>(0xE0U | (codepoint >> 12U));
    result.bytes[1] = static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU));
    result.bytes[2] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    result.size = 3U;
  } else if (codepoint >= 0x10000U && codepoint <= 0x10FFFFU) {
    result.bytes[0] = static_cast<char>(0xF0U | (codepoint >> 18U));
    result.bytes[1] = static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU));
    result.bytes[2] = static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU));
    result.bytes[3] = static_cast<char>(0x80U | (codepoint & 0x3FU));
    result.size = 4U;
  }
  return result;
}

}  // namespace hmi::text::utf8
