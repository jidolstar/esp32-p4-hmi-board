#include <array>
#include <cassert>
#include <cstring>

#include "hmi/i18n/format.hpp"

int main() {
  using hmi::i18n::FormatArgument;
  using hmi::i18n::FormatStatus;

  std::array<char, 96U> output{};
  constexpr std::array arguments = {
      FormatArgument::number("count", 12U),
      FormatArgument::number("max", 64U),
      FormatArgument::text("name", "HMI"),
  };
  auto result = hmi::i18n::format_to(
      "{name:string}: {count:u32}/{max:u32}", arguments.data(), arguments.size(),
      output.data(), output.size());
  assert(result.status == FormatStatus::ok);
  assert(std::strcmp(output.data(), "HMI: 12/64") == 0);

  std::strcpy(output.data(), "unchanged");
  result = hmi::i18n::format_to("{missing:u32}", arguments.data(),
                                arguments.size(), output.data(), output.size());
  assert(result.status == FormatStatus::missing_argument);
  assert(std::strcmp(output.data(), "unchanged") == 0);

  result = hmi::i18n::format_to("{count:string}", arguments.data(),
                                arguments.size(), output.data(), output.size());
  assert(result.status == FormatStatus::type_mismatch);

  std::array<char, 4U> small{{'k', 'e', 'e', 'p'}};
  result = hmi::i18n::format_to("1234", nullptr, 0U, small.data(), small.size());
  assert(result.status == FormatStatus::output_too_small);
  assert(small[0] == 'k');
  return 0;
}

