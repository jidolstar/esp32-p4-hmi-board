#include "hmi/text/keyboard_layout.hpp"

#include <array>

namespace hmi::text {
namespace {

constexpr std::array<KeyboardKey, 26U> kHangul = {{{"ㅂ", 0x3142U}, {"ㅈ", 0x3148U}, {"ㄷ", 0x3137U}, {"ㄱ", 0x3131U}, {"ㅅ", 0x3145U}, {"ㅛ", 0x315BU}, {"ㅕ", 0x3155U}, {"ㅑ", 0x3151U}, {"ㅐ", 0x3150U}, {"ㅔ", 0x3154U}, {"ㅁ", 0x3141U}, {"ㄴ", 0x3134U}, {"ㅇ", 0x3147U}, {"ㄹ", 0x3139U}, {"ㅎ", 0x314EU}, {"ㅗ", 0x3157U}, {"ㅓ", 0x3153U}, {"ㅏ", 0x314FU}, {"ㅣ", 0x3163U}, {"ㅋ", 0x314BU}, {"ㅌ", 0x314CU}, {"ㅊ", 0x314AU}, {"ㅍ", 0x314DU}, {"ㅠ", 0x3160U}, {"ㅜ", 0x315CU}, {"ㅡ", 0x3161U}}};

constexpr std::array<KeyboardKey, 26U> kHangulShift = {{{"ㅃ", 0x3143U}, {"ㅉ", 0x3149U}, {"ㄸ", 0x3138U}, {"ㄲ", 0x3132U}, {"ㅆ", 0x3146U}, {"ㅛ", 0x315BU}, {"ㅕ", 0x3155U}, {"ㅑ", 0x3151U}, {"ㅒ", 0x3152U}, {"ㅖ", 0x3156U}, {"ㅁ", 0x3141U}, {"ㄴ", 0x3134U}, {"ㅇ", 0x3147U}, {"ㄹ", 0x3139U}, {"ㅎ", 0x314EU}, {"ㅗ", 0x3157U}, {"ㅓ", 0x3153U}, {"ㅏ", 0x314FU}, {"ㅣ", 0x3163U}, {"ㅋ", 0x314BU}, {"ㅌ", 0x314CU}, {"ㅊ", 0x314AU}, {"ㅍ", 0x314DU}, {"ㅠ", 0x3160U}, {"ㅜ", 0x315CU}, {"ㅡ", 0x3161U}}};

constexpr std::array<KeyboardKey, 26U> kLatinLower = {{{"q", 'q'}, {"w", 'w'}, {"e", 'e'}, {"r", 'r'}, {"t", 't'}, {"y", 'y'}, {"u", 'u'}, {"i", 'i'}, {"o", 'o'}, {"p", 'p'}, {"a", 'a'}, {"s", 's'}, {"d", 'd'}, {"f", 'f'}, {"g", 'g'}, {"h", 'h'}, {"j", 'j'}, {"k", 'k'}, {"l", 'l'}, {"z", 'z'}, {"x", 'x'}, {"c", 'c'}, {"v", 'v'}, {"b", 'b'}, {"n", 'n'}, {"m", 'm'}}};

constexpr std::array<KeyboardKey, 26U> kLatinUpper = {{{"Q", 'Q'}, {"W", 'W'}, {"E", 'E'}, {"R", 'R'}, {"T", 'T'}, {"Y", 'Y'}, {"U", 'U'}, {"I", 'I'}, {"O", 'O'}, {"P", 'P'}, {"A", 'A'}, {"S", 'S'}, {"D", 'D'}, {"F", 'F'}, {"G", 'G'}, {"H", 'H'}, {"J", 'J'}, {"K", 'K'}, {"L", 'L'}, {"Z", 'Z'}, {"X", 'X'}, {"C", 'C'}, {"V", 'V'}, {"B", 'B'}, {"N", 'N'}, {"M", 'M'}}};

constexpr std::array<KeyboardKey, 10U> kNumbers = {{{"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'}, {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'}, {"0", '0'}}};

constexpr std::array<KeyboardKey, 16U> kSymbols = {{{"!", '!'}, {"@", '@'}, {"#", '#'}, {"$", '$'}, {"%", '%'}, {"&", '&'}, {"*", '*'}, {"(", '('}, {")", ')'}, {"-", '-'}, {"_", '_'}, {"+", '+'}, {"=", '='}, {"?", '?'}, {".", '.'}, {",", ','}}};

constexpr std::array<KeyboardRowDescriptor, 3U> kLetterRows = {{
    {0U, 10U, 0U, 2U},
    {10U, 9U, 1U, 2U},
    {19U, 7U, 3U, 2U},
}};

constexpr std::array<KeyboardRowDescriptor, 1U> kNumberRows = {{{0U, 10U, 0U, 2U}}};

constexpr std::array<KeyboardRowDescriptor, 3U> kSymbolRows = {{
    {0U, 6U, 0U, 2U},
    {6U, 5U, 1U, 2U},
    {11U, 5U, 1U, 2U},
}};

template <std::size_t KeyCount, std::size_t RowCount>
KeyboardLayoutView view(const std::array<KeyboardKey, KeyCount>& keys,
                        const std::array<KeyboardRowDescriptor, RowCount>& rows) noexcept {
  return {keys.data(), keys.size(), rows.data(), rows.size()};
}

}  // namespace

KeyboardLayoutView keyboard_layout(KeyboardMode mode, bool shift) noexcept {
  switch (mode) {
    case KeyboardMode::hangul:
      return shift ? view(kHangulShift, kLetterRows) : view(kHangul, kLetterRows);
    case KeyboardMode::latin:
      return shift ? view(kLatinUpper, kLetterRows) : view(kLatinLower, kLetterRows);
    case KeyboardMode::number:
      return view(kNumbers, kNumberRows);
    case KeyboardMode::symbol:
      return view(kSymbols, kSymbolRows);
  }
  return {};
}

}  // namespace hmi::text
