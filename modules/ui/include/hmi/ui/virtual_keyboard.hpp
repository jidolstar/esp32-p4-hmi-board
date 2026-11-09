#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "hmi/text/keyboard_layout.hpp"
#include "hmi/ui/font_repository.hpp"
#include "hmi/ui/theme.hpp"

namespace hmi::ui {

enum class VirtualKeyAction : std::uint8_t {
  layout_character,
  literal_character,
  shift,
  symbol,
  language,
  space,
  backspace,
  submit,
};

struct VirtualKeyEvent {
  VirtualKeyAction action = VirtualKeyAction::literal_character;
  std::uint32_t codepoint = 0U;
  std::size_t layout_index = 0U;
};

struct VirtualKeyboardCaptions {
  const char* shift = nullptr;
  const char* symbol = nullptr;
  const char* language = nullptr;
  const char* space = nullptr;
  const char* backspace = nullptr;
  const char* submit = nullptr;
};

/**
 * 숫자행·문자 3행·기능행을 고정 배치하고 semantic key event만 방출한다.
 *
 * editor, field 순서와 modal 정책은 소유하지 않는다. 동적 문자 font와 theme는 이
 * 객체보다 오래 살아야 하며 caption 문자열은 render 호출 중에만 유효하면 된다.
 */
class VirtualKeyboard {
 public:
  static constexpr std::size_t kMaxKeys = 48U;
  static constexpr std::size_t kRowCount = 5U;
  static constexpr lv_coord_t kMinimumKeyHeight = 44;

  using EventHandler = void (*)(void* context, const VirtualKeyEvent& event) noexcept;

  VirtualKeyboard() = default;
  VirtualKeyboard(const VirtualKeyboard&) = delete;
  VirtualKeyboard& operator=(const VirtualKeyboard&) = delete;
  ~VirtualKeyboard();

  bool create(lv_obj_t* parent, FontRepository& fonts, UiTheme& theme,
              const lv_font_t* action_font, lv_coord_t width,
              lv_coord_t height) noexcept;
  void destroy() noexcept;
  bool render(hmi::text::KeyboardMode mode, bool shift,
              const VirtualKeyboardCaptions& captions) noexcept;
  void set_event_handler(EventHandler handler, void* context) noexcept;

  [[nodiscard]] lv_obj_t* object() const noexcept { return root_; }
  [[nodiscard]] std::size_t key_count() const noexcept { return key_count_; }
  [[nodiscard]] std::size_t row_count() const noexcept { return kRowCount; }
  [[nodiscard]] lv_coord_t row_height() const noexcept { return row_height_; }
  [[nodiscard]] VirtualKeyEvent key_event(std::size_t index) const noexcept;
  [[nodiscard]] std::uint8_t key_row(std::size_t index) const noexcept;
  [[nodiscard]] lv_obj_t* key_object(std::size_t index) const noexcept;

 private:
  struct KeySlot {
    lv_obj_t* button = nullptr;
    std::uint32_t codepoint = 0U;
    std::uint8_t layout_index = 0U;
    std::uint8_t row = 0U;
    VirtualKeyAction action = VirtualKeyAction::literal_character;
  };

  static void handle_event(lv_event_t* event) noexcept;
  bool create_rows() noexcept;
  bool set_leading_inset(lv_obj_t* row, lv_coord_t width) noexcept;
  bool add_dynamic_key(lv_obj_t* row, std::uint8_t row_index, lv_coord_t width,
                       VirtualKeyAction action, std::uint32_t codepoint,
                       std::size_t layout_index,
                       ButtonVariant variant = ButtonVariant::kSecondary) noexcept;
  bool add_action_key(lv_obj_t* row, std::uint8_t row_index, lv_coord_t width,
                      VirtualKeyAction action, const char* caption,
                      ButtonVariant variant) noexcept;
  bool add_layout_row(std::uint8_t visual_row, const hmi::text::KeyboardLayoutView& layout,
                      std::size_t descriptor_index, bool include_shift,
                      bool include_submit, const VirtualKeyboardCaptions& captions,
                      bool shift_active) noexcept;

  lv_obj_t* root_ = nullptr;
  std::array<lv_obj_t*, kRowCount> rows_{};
  std::array<KeySlot, kMaxKeys> keys_{};
  FontRepository* fonts_ = nullptr;
  UiTheme* theme_ = nullptr;
  const lv_font_t* action_font_ = nullptr;
  EventHandler handler_ = nullptr;
  void* handler_context_ = nullptr;
  std::size_t key_count_ = 0U;
  lv_coord_t width_ = 0;
  lv_coord_t height_ = 0;
  lv_coord_t row_height_ = 0;
  lv_coord_t key_width_ = 0;
};

static_assert(sizeof(VirtualKeyboard) <= 1024U,
              "VirtualKeyboard must keep fixed-capacity key metadata only");

}  // namespace hmi::ui
