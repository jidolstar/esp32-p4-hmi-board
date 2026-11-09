#include "hmi/text/hangul_composer.hpp"

#include <array>
#include <utility>

namespace hmi::text {
namespace {

constexpr std::array<std::uint32_t, 19U> kLeadingJamo = {
    0x3131U, 0x3132U, 0x3134U, 0x3137U, 0x3138U, 0x3139U, 0x3141U,
    0x3142U, 0x3143U, 0x3145U, 0x3146U, 0x3147U, 0x3148U, 0x3149U,
    0x314AU, 0x314BU, 0x314CU, 0x314DU, 0x314EU,
};

constexpr std::array<std::uint32_t, 21U> kVowelJamo = {
    0x314FU, 0x3150U, 0x3151U, 0x3152U, 0x3153U, 0x3154U, 0x3155U,
    0x3156U, 0x3157U, 0x3158U, 0x3159U, 0x315AU, 0x315BU, 0x315CU,
    0x315DU, 0x315EU, 0x315FU, 0x3160U, 0x3161U, 0x3162U, 0x3163U,
};

template <typename Values>
constexpr int find_index(const Values& values, std::uint32_t codepoint) noexcept {
  for (std::size_t index = 0U; index < values.size(); ++index) {
    if (values[index] == codepoint) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

constexpr int combine_leading(int first, int second) noexcept {
  if (first == second) {
    switch (first) {
      case 0:
        return 1;
      case 3:
        return 4;
      case 7:
        return 8;
      case 9:
        return 10;
      case 12:
        return 13;
      default:
        break;
    }
  }
  return -1;
}

constexpr int split_leading(int leading) noexcept {
  switch (leading) {
    case 1:
      return 0;
    case 4:
      return 3;
    case 8:
      return 7;
    case 10:
      return 9;
    case 13:
      return 12;
    default:
      return -1;
  }
}

constexpr int combine_vowel(int first, int second) noexcept {
  if (second == 20) {
    switch (first) {
      case 0:
        return 1;
      case 2:
        return 3;
      case 4:
        return 5;
      case 6:
        return 7;
      case 8:
        return 11;
      case 13:
        return 16;
      case 18:
        return 19;
      default:
        break;
    }
  }
  if (first == 8 && second == 0) {
    return 9;
  }
  if (first == 8 && second == 1) {
    return 10;
  }
  if (first == 13 && second == 4) {
    return 14;
  }
  if (first == 13 && second == 5) {
    return 15;
  }
  return -1;
}

constexpr int split_vowel(int vowel) noexcept {
  switch (vowel) {
    case 1:
    case 3:
      return vowel - 1;
    case 5:
    case 7:
      return vowel - 1;
    case 9:
    case 10:
    case 11:
      return 8;
    case 14:
    case 15:
    case 16:
      return 13;
    case 19:
      return 18;
    default:
      return -1;
  }
}

constexpr int trailing_from_leading(int leading) noexcept {
  constexpr std::array<std::int8_t, 19U> mapping = {
      1, 2, 4, 7, 0, 8, 16, 17, 0, 19, 20, 21, 22, 0, 23, 24, 25, 26, 27,
  };
  return leading >= 0 && leading < static_cast<int>(mapping.size()) ? mapping[leading] : 0;
}

constexpr int leading_from_trailing(int trailing) noexcept {
  constexpr std::array<std::int8_t, 28U> mapping = {
      -1, 0, 1, -1, 2, -1, -1, 3, 5, -1, -1, -1, -1, -1,
      -1, -1, 6, 7, -1, 9, 10, 11, 12, 14, 15, 16, 17, 18,
  };
  return trailing > 0 && trailing < static_cast<int>(mapping.size()) ? mapping[trailing] : -1;
}

constexpr int combine_trailing(int first, int next_leading) noexcept {
  if (first == 1 && next_leading == 9) return 3;
  if (first == 4 && next_leading == 12) return 5;
  if (first == 4 && next_leading == 18) return 6;
  if (first == 8 && next_leading == 0) return 9;
  if (first == 8 && next_leading == 6) return 10;
  if (first == 8 && next_leading == 7) return 11;
  if (first == 8 && next_leading == 9) return 12;
  if (first == 8 && next_leading == 16) return 13;
  if (first == 8 && next_leading == 17) return 14;
  if (first == 8 && next_leading == 18) return 15;
  if (first == 17 && next_leading == 9) return 18;
  return 0;
}

constexpr std::pair<int, int> split_trailing(int trailing) noexcept {
  switch (trailing) {
    case 3:
      return {1, 9};
    case 5:
      return {4, 12};
    case 6:
      return {4, 18};
    case 9:
      return {8, 0};
    case 10:
      return {8, 6};
    case 11:
      return {8, 7};
    case 12:
      return {8, 9};
    case 13:
      return {8, 16};
    case 14:
      return {8, 17};
    case 15:
      return {8, 18};
    case 18:
      return {17, 9};
    default:
      return {0, -1};
  }
}

constexpr std::uint32_t compose_syllable(int leading, int vowel, int trailing) noexcept {
  return 0xAC00U + static_cast<std::uint32_t>((leading * 21 + vowel) * 28 + trailing);
}

}  // namespace

bool HangulComposer::is_composing() const noexcept {
  return leading_ >= 0 || vowel_ >= 0;
}

std::uint32_t HangulComposer::preview_codepoint() const noexcept {
  if (leading_ >= 0 && vowel_ >= 0) {
    return compose_syllable(leading_, vowel_, trailing_);
  }
  if (leading_ >= 0) {
    return kLeadingJamo[static_cast<std::size_t>(leading_)];
  }
  if (vowel_ >= 0) {
    return kVowelJamo[static_cast<std::size_t>(vowel_)];
  }
  return 0U;
}

EditStatus HangulComposer::commit(Editor& editor) noexcept {
  if (!is_composing()) {
    return EditStatus::ok;
  }
  const auto status = editor.insert_codepoint(preview_codepoint());
  if (status == EditStatus::ok) {
    cancel();
  }
  return status;
}

EditStatus HangulComposer::input_jamo(Editor& editor,
                                      std::uint32_t compatibility_jamo) noexcept {
  const int next_leading = find_index(kLeadingJamo, compatibility_jamo);
  const int next_vowel = find_index(kVowelJamo, compatibility_jamo);
  if (next_leading < 0 && next_vowel < 0) {
    return EditStatus::invalid_codepoint;
  }

  if (next_vowel >= 0) {
    if (vowel_ < 0) {
      vowel_ = static_cast<std::int8_t>(next_vowel);
      return EditStatus::ok;
    }

    if (trailing_ > 0) {
      const auto split = split_trailing(trailing_);
      const int carried_leading = split.second >= 0 ? split.second : leading_from_trailing(trailing_);
      const int committed_trailing = split.second >= 0 ? split.first : 0;
      const auto status = editor.insert_codepoint(
          compose_syllable(leading_, vowel_, committed_trailing));
      if (status != EditStatus::ok) {
        return status;
      }
      leading_ = static_cast<std::int8_t>(carried_leading);
      vowel_ = static_cast<std::int8_t>(next_vowel);
      trailing_ = 0;
      return EditStatus::ok;
    }

    const int combined = combine_vowel(vowel_, next_vowel);
    if (combined >= 0) {
      vowel_ = static_cast<std::int8_t>(combined);
      return EditStatus::ok;
    }

    const auto status = commit(editor);
    if (status != EditStatus::ok) {
      return status;
    }
    vowel_ = static_cast<std::int8_t>(next_vowel);
    return EditStatus::ok;
  }

  if (leading_ < 0 && vowel_ < 0) {
    leading_ = static_cast<std::int8_t>(next_leading);
    return EditStatus::ok;
  }

  if (leading_ < 0 && vowel_ >= 0) {
    const auto status = commit(editor);
    if (status != EditStatus::ok) {
      return status;
    }
    leading_ = static_cast<std::int8_t>(next_leading);
    return EditStatus::ok;
  }

  if (vowel_ < 0) {
    const int combined = combine_leading(leading_, next_leading);
    if (combined >= 0) {
      leading_ = static_cast<std::int8_t>(combined);
      return EditStatus::ok;
    }
    const auto status = commit(editor);
    if (status != EditStatus::ok) {
      return status;
    }
    leading_ = static_cast<std::int8_t>(next_leading);
    return EditStatus::ok;
  }

  const int next_trailing = trailing_from_leading(next_leading);
  if (trailing_ == 0 && next_trailing > 0) {
    trailing_ = static_cast<std::int8_t>(next_trailing);
    return EditStatus::ok;
  }
  if (trailing_ > 0) {
    const int combined = combine_trailing(trailing_, next_leading);
    if (combined > 0) {
      trailing_ = static_cast<std::int8_t>(combined);
      return EditStatus::ok;
    }
  }

  const auto status = commit(editor);
  if (status != EditStatus::ok) {
    return status;
  }
  leading_ = static_cast<std::int8_t>(next_leading);
  return EditStatus::ok;
}

EditStatus HangulComposer::backspace(Editor& editor) noexcept {
  if (trailing_ > 0) {
    const auto split = split_trailing(trailing_);
    trailing_ = static_cast<std::int8_t>(split.second >= 0 ? split.first : 0);
    return EditStatus::ok;
  }
  if (vowel_ >= 0) {
    const int split = split_vowel(vowel_);
    if (split >= 0) {
      vowel_ = static_cast<std::int8_t>(split);
    } else {
      vowel_ = -1;
    }
    return EditStatus::ok;
  }
  if (leading_ >= 0) {
    const int split = split_leading(leading_);
    leading_ = static_cast<std::int8_t>(split);
    return EditStatus::ok;
  }
  return editor.backspace();
}

void HangulComposer::cancel() noexcept {
  leading_ = -1;
  vowel_ = -1;
  trailing_ = 0;
}

}  // namespace hmi::text
