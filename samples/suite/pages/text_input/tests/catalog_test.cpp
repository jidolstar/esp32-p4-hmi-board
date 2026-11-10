#include <cassert>
#include <cstddef>
#include <string_view>

#include "hmi/i18n/catalog.hpp"
#include "hmi/i18n/locale.hpp"
#include "generated/text_catalog.hpp"

namespace {

namespace catalog = hmi::samples::suite::generated;

void verify_locale_contract() {
  using hmi::i18n::Locale;

  assert(hmi::i18n::locale_tag(Locale::ko_kr) == "ko-KR");
  assert(hmi::i18n::locale_tag(Locale::en_us) == "en-US");
  assert(hmi::i18n::parse_locale("ko-KR").matched);
  assert(hmi::i18n::parse_locale("en-US").matched);
  assert(!hmi::i18n::parse_locale("ko").matched);
}

void verify_generated_catalog() {
  using hmi::i18n::Locale;

  static_assert(sizeof(Locale) == 1U, "Locale must remain a one-byte value");
  static_assert(sizeof(hmi::i18n::CatalogView) <= sizeof(void*) * 2U,
                "CatalogView must not own catalog strings");
  static_assert(catalog::kTextCount == 283U, "Unexpected text catalog size");

  const auto korean = catalog::catalog(Locale::ko_kr);
  const auto english = catalog::catalog(Locale::en_us);
  assert(korean.size() == catalog::kTextCount);
  assert(english.size() == catalog::kTextCount);

  for (std::size_t index = 0U; index < catalog::kTextCount; ++index) {
    assert(!korean.get(index).empty());
    assert(!english.get(index).empty());
    assert(!catalog::kKeyNames[index].empty());
  }

  assert(catalog::text(Locale::ko_kr, catalog::TextId::page_text_input_title) == "텍스트 입력");
  assert(catalog::text(Locale::en_us, catalog::TextId::page_text_input_title) == "Text Input");
  assert(catalog::text(Locale::ko_kr, "page.ui_basics.title") == "UI 기본");
  assert(catalog::text(Locale::en_us, "page.ui_basics.title") == "UI Basics");
  assert(catalog::text(Locale::en_us, "page.unknown.title").empty());
  assert(catalog::text(Locale::ko_kr,
                       catalog::TextId::text_input_keyboard_next) == "다음");
  assert(catalog::text(Locale::en_us,
                       catalog::TextId::text_input_keyboard_space) == "Space");
  assert(catalog::text(Locale::ko_kr,
                       catalog::TextId::wifi_scan_selected) == "선택됨");
  assert(catalog::text(Locale::ko_kr,
                       catalog::TextId::wifi_scan_connected) == "연결됨");
  assert(catalog::text(Locale::en_us,
                       catalog::TextId::wifi_editor_selected_title) ==
         "Selected network");
  assert(catalog::text(Locale::ko_kr,
                       catalog::TextId::page_wav_sounds_title) == "등록 WAV");
  assert(catalog::text(Locale::en_us,
                       catalog::TextId::wav_sounds_action_replay) ==
         "Play again");
  assert(catalog::placeholder_contract(
             catalog::TextId::text_input_status_length) ==
         "bytes:u32,count:u32,max:u32");
}

}  // namespace

int main() {
  verify_locale_contract();
  verify_generated_catalog();
  return 0;
}
