#include <cassert>

#include "hmi/text/keyboard_layout.hpp"

int main() {
  using hmi::text::KeyboardMode;
  using hmi::text::keyboard_layout;

  const auto hangul = keyboard_layout(KeyboardMode::hangul, false);
  const auto hangul_shift = keyboard_layout(KeyboardMode::hangul, true);
  const auto latin = keyboard_layout(KeyboardMode::latin, false);
  const auto latin_shift = keyboard_layout(KeyboardMode::latin, true);
  const auto numbers = keyboard_layout(KeyboardMode::number, false);
  const auto symbols = keyboard_layout(KeyboardMode::symbol, false);

  assert(hangul.size == 26U);
  assert(hangul.keys[0].codepoint == 0x3142U);        // ㅂ
  assert(hangul_shift.keys[0].codepoint == 0x3143U);  // ㅃ
  assert(latin.keys[0].codepoint == 'q');
  assert(latin_shift.keys[0].codepoint == 'Q');
  assert(numbers.size == 10U);
  assert(symbols.size == 16U);
  assert(keyboard_layout(KeyboardMode::number, true).keys == numbers.keys);

  assert(hangul.row_count == 3U);
  assert(hangul.rows[0].key_offset == 0U);
  assert(hangul.rows[0].key_count == 10U);
  assert(hangul.rows[0].leading_half_units == 0U);
  assert(hangul.rows[1].key_offset == 10U);
  assert(hangul.rows[1].key_count == 9U);
  assert(hangul.rows[1].leading_half_units == 1U);
  assert(hangul.rows[2].key_offset == 19U);
  assert(hangul.rows[2].key_count == 7U);
  assert(hangul.rows[2].leading_half_units == 3U);
  assert(hangul.rows[2].key_width_half_units == 2U);

  assert(latin.rows == hangul.rows);
  assert(latin_shift.rows == hangul.rows);
  assert(numbers.row_count == 1U);
  assert(numbers.rows[0].key_count == 10U);
  assert(symbols.row_count == 3U);
  assert(symbols.rows[0].key_count == 6U);
  assert(symbols.rows[1].key_offset == 6U);
  assert(symbols.rows[1].key_count == 5U);
  assert(symbols.rows[2].key_offset == 11U);
  assert(symbols.rows[2].key_count == 5U);

  // 두벌식/QWERTY의 10/9/7 순서가 일반 키보드 행과 일치한다.
  assert(hangul.keys[9].codepoint == 0x3154U);   // ㅔ
  assert(hangul.keys[10].codepoint == 0x3141U);  // ㅁ
  assert(hangul.keys[18].codepoint == 0x3163U);  // ㅣ
  assert(hangul.keys[19].codepoint == 0x314BU);  // ㅋ
  assert(hangul.keys[25].codepoint == 0x3161U);  // ㅡ
  assert(latin.keys[9].codepoint == 'p');
  assert(latin.keys[10].codepoint == 'a');
  assert(latin.keys[18].codepoint == 'l');
  assert(latin.keys[19].codepoint == 'z');
  assert(latin.keys[25].codepoint == 'm');
  return 0;
}
