#include "hmi/i18n/locale.hpp"

namespace hmi::i18n {

std::string_view locale_tag(Locale locale) noexcept {
  switch (locale) {
    case Locale::ko_kr:
      return "ko-KR";
    case Locale::en_us:
      return "en-US";
  }
  return {};
}

LocaleParseResult parse_locale(std::string_view tag) noexcept {
  if (tag == "ko-KR") {
    return {Locale::ko_kr, true};
  }
  if (tag == "en-US") {
    return {Locale::en_us, true};
  }
  return {};
}

}  // namespace hmi::i18n
