#pragma once

#include <cstdint>
#include <string_view>

namespace hmi::i18n {

/** 화면 고정 문구에 적용하는 지원 locale이다. */
enum class Locale : std::uint8_t {
  ko_kr,
  en_us,
};

struct LocaleParseResult {
  Locale locale = Locale::ko_kr;
  bool matched = false;
};

/** locale을 BCP 47 language tag로 반환한다. */
std::string_view locale_tag(Locale locale) noexcept;

/** 지원하는 language tag를 locale 값으로 변환하며 fallback을 수행하지 않는다. */
LocaleParseResult parse_locale(std::string_view tag) noexcept;

}  // namespace hmi::i18n
