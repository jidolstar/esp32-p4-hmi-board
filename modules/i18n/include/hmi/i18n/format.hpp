#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hmi::i18n {

enum class FormatValueType : std::uint8_t { u32, string };

struct FormatArgument {
  std::string_view name{};
  FormatValueType type = FormatValueType::string;
  std::uint32_t u32 = 0U;
  std::string_view string{};

  static constexpr FormatArgument number(std::string_view name,
                                         std::uint32_t value) noexcept {
    return {name, FormatValueType::u32, value, {}};
  }
  static constexpr FormatArgument text(std::string_view name,
                                       std::string_view value) noexcept {
    return {name, FormatValueType::string, 0U, value};
  }
};

enum class FormatStatus : std::uint8_t {
  ok,
  invalid_template,
  missing_argument,
  type_mismatch,
  output_too_small,
};

struct FormatResult {
  FormatStatus status = FormatStatus::invalid_template;
  std::size_t size = 0U;
};

/**
 * `{name:u32}`와 `{name:string}` placeholder를 caller-owned 고정 buffer에 쓴다.
 *
 * 실패하면 output을 변경하지 않는다. locale 문장을 코드에서 조각내지 않고 catalog의
 * 어순을 그대로 유지하기 위한 bounded formatter이며 heap을 사용하지 않는다.
 */
FormatResult format_to(std::string_view pattern, const FormatArgument* arguments,
                       std::size_t argument_count, char* output,
                       std::size_t output_capacity) noexcept;

}  // namespace hmi::i18n

