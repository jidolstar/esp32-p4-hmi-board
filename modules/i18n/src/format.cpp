#include "hmi/i18n/format.hpp"

#include <array>
#include <charconv>
#include <cstring>

namespace hmi::i18n {
namespace {

struct PassResult {
  FormatStatus status = FormatStatus::ok;
  std::size_t size = 0U;
};

const FormatArgument* find_argument(const FormatArgument* arguments,
                                    std::size_t count,
                                    std::string_view name) noexcept {
  for (std::size_t index = 0U; index < count; ++index) {
    if (arguments[index].name == name) {
      return &arguments[index];
    }
  }
  return nullptr;
}

PassResult run_pass(std::string_view pattern, const FormatArgument* arguments,
                    std::size_t argument_count, char* output) noexcept {
  std::size_t read = 0U;
  std::size_t written = 0U;
  while (read < pattern.size()) {
    if (pattern[read] != '{') {
      if (pattern[read] == '}') {
        return {FormatStatus::invalid_template, 0U};
      }
      if (output != nullptr) {
        output[written] = pattern[read];
      }
      ++read;
      ++written;
      continue;
    }
    const std::size_t close = pattern.find('}', read + 1U);
    if (close == std::string_view::npos) {
      return {FormatStatus::invalid_template, 0U};
    }
    const std::string_view placeholder =
        pattern.substr(read + 1U, close - read - 1U);
    const std::size_t separator = placeholder.find(':');
    if (separator == std::string_view::npos || separator == 0U ||
        separator + 1U >= placeholder.size()) {
      return {FormatStatus::invalid_template, 0U};
    }
    const std::string_view name = placeholder.substr(0U, separator);
    const std::string_view type = placeholder.substr(separator + 1U);
    const FormatArgument* argument =
        find_argument(arguments, argument_count, name);
    if (argument == nullptr) {
      return {FormatStatus::missing_argument, 0U};
    }

    std::array<char, 10U> digits{};
    std::string_view replacement;
    if (type == "u32") {
      if (argument->type != FormatValueType::u32) {
        return {FormatStatus::type_mismatch, 0U};
      }
      const auto converted =
          std::to_chars(digits.data(), digits.data() + digits.size(), argument->u32);
      if (converted.ec != std::errc{}) {
        return {FormatStatus::invalid_template, 0U};
      }
      replacement = {digits.data(),
                     static_cast<std::size_t>(converted.ptr - digits.data())};
    } else if (type == "string") {
      if (argument->type != FormatValueType::string) {
        return {FormatStatus::type_mismatch, 0U};
      }
      replacement = argument->string;
    } else {
      return {FormatStatus::invalid_template, 0U};
    }
    if (output != nullptr && !replacement.empty()) {
      std::memcpy(output + written, replacement.data(), replacement.size());
    }
    written += replacement.size();
    read = close + 1U;
  }
  return {FormatStatus::ok, written};
}

}  // namespace

FormatResult format_to(std::string_view pattern, const FormatArgument* arguments,
                       std::size_t argument_count, char* output,
                       std::size_t output_capacity) noexcept {
  if ((argument_count > 0U && arguments == nullptr) || output == nullptr ||
      output_capacity == 0U) {
    return {FormatStatus::output_too_small, 0U};
  }
  const PassResult measured =
      run_pass(pattern, arguments, argument_count, nullptr);
  if (measured.status != FormatStatus::ok) {
    return {measured.status, 0U};
  }
  if (measured.size + 1U > output_capacity) {
    return {FormatStatus::output_too_small, measured.size};
  }
  const PassResult rendered = run_pass(pattern, arguments, argument_count, output);
  if (rendered.status != FormatStatus::ok) {
    return {rendered.status, 0U};
  }
  output[rendered.size] = '\0';
  return {FormatStatus::ok, rendered.size};
}

}  // namespace hmi::i18n

