#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lvgl.h>

namespace hmi::ui {

enum class ThemeMode : std::uint8_t { kDark, kLight };
enum class ButtonVariant : std::uint8_t { kPrimary, kSecondary, kGhost };
enum class TextRole : std::uint8_t {
  kDisplay,
  kTitle,
  kBody,
  kMuted,
  kInverse,
  kAccent,
};

/**
 * 앱 수명 동안 유지되는 공통 UI 테마다.
 *
 * 스타일 객체를 참조하는 LVGL widget보다 오래 살아야 한다. 모드 변경 전에는 기존
 * widget을 모두 제거해야 하며, set_mode는 그 안전한 전환 지점에서만 호출한다.
 */
class UiTheme {
 public:
  UiTheme() = default;
  UiTheme(const UiTheme &) = delete;
  UiTheme &operator=(const UiTheme &) = delete;

  bool initialize(ThemeMode mode = ThemeMode::kDark) noexcept;
  bool set_mode(ThemeMode mode) noexcept;
  void reset() noexcept;
  ~UiTheme();

  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  [[nodiscard]] ThemeMode mode() const noexcept { return mode_; }
  [[nodiscard]] ThemeMode opposite_mode() const noexcept;
  [[nodiscard]] lv_color_t text_color() const noexcept;
  [[nodiscard]] lv_color_t accent_color() const noexcept;

  void apply_screen(lv_obj_t *object) noexcept;
  void apply_header(lv_obj_t *object) noexcept;
  void apply_surface(lv_obj_t *object) noexcept;
  void apply_card(lv_obj_t *object) noexcept;
  void apply_button(lv_obj_t *object, ButtonVariant variant) noexcept;
  void apply_label(lv_obj_t *label, TextRole role) noexcept;
  void apply_chip(lv_obj_t *object) noexcept;
  void apply_status(lv_obj_t *object) noexcept;
  void apply_list_item(lv_obj_t *object) noexcept;
  void apply_toast(lv_obj_t *object) noexcept;
  void apply_scrim(lv_obj_t *object) noexcept;
  void apply_dialog(lv_obj_t *object) noexcept;
  void apply_transparent(lv_obj_t *object) noexcept;

 private:
  enum class StyleId : std::size_t {
    kScreen,
    kHeader,
    kSurface,
    kCard,
    kCardPressed,
    kButtonPrimary,
    kButtonPrimaryPressed,
    kButtonSecondary,
    kButtonSecondaryPressed,
    kButtonGhost,
    kButtonGhostPressed,
    kTextDisplay,
    kTextTitle,
    kTextBody,
    kTextMuted,
    kTextInverse,
    kTextAccent,
    kChip,
    kStatus,
    kListItem,
    kToast,
    kScrim,
    kDialog,
    kTransparent,
    kCount,
  };

  static constexpr std::size_t kStyleCount =
      static_cast<std::size_t>(StyleId::kCount);

  lv_style_t &style(StyleId id) noexcept;
  void configure() noexcept;

  std::array<lv_style_t, kStyleCount> styles_{};
  ThemeMode mode_ = ThemeMode::kDark;
  bool initialized_ = false;
};

static_assert(sizeof(UiTheme) <= 2048U,
              "UiTheme must remain a bounded style owner");

}  // namespace hmi::ui
