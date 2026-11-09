#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lvgl.h>

#include "hmi/i18n/locale.hpp"
#include "hmi/text/input_model.hpp"
#include "hmi/ui/font_repository.hpp"
#include "hmi/ui/user_text_input_line.hpp"
#include "hmi/ui/user_text_label.hpp"
#include "hmi/ui/virtual_keyboard.hpp"
#include "page_contract.hpp"
#include "generated/text_catalog.hpp"

namespace hmi::samples::suite {

enum class TextFieldId : std::uint8_t { name, memo, value };
enum class TextInputFlowState : std::uint8_t { editing, review, confirmed };

struct TextInputSnapshot {
  hmi::i18n::Locale locale = hmi::i18n::Locale::ko_kr;
  hmi::text::KeyboardMode keyboard_mode = hmi::text::KeyboardMode::hangul;
  TextFieldId active_field = TextFieldId::name;
  std::size_t codepoints = 0U;
  std::size_t bytes = 0U;
  std::size_t cursor = 0U;
  bool composing = false;
  TextInputFlowState flow = TextInputFlowState::editing;
};

/**
 * 한·영 고정 문구와 세 개의 bounded 입력 model을 조립하는 상품 text-input page다.
 *
 * locale은 SampleSuiteApp이 소유하고 이 page는 변경 callback으로만 갱신한다. 입력값,
 * cursor와 미완성 조합은 page 소유이므로 Dashboard 이탈 시 모두 폐기된다. 동적 font
 * widget을 먼저 삭제한 뒤 repository를 reset해 backing data lifetime을 보장한다.
 */
class TextInputPage final : public SamplePage {
 public:
  TextInputPage() = default;
  TextInputPage(const TextInputPage&) = delete;
  TextInputPage& operator=(const TextInputPage&) = delete;
  ~TextInputPage() override;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override { return {}; }
  bool handle_utf8_input(const char* utf8) noexcept override;
  bool handle_key(hmi::profiles::AppKey key) noexcept override;

  [[nodiscard]] TextInputSnapshot text_snapshot() const noexcept;
  bool select_field(TextFieldId field) noexcept;
  bool set_locale(hmi::i18n::Locale locale) noexcept;
  bool press_character(std::size_t key_index) noexcept;
  bool set_keyboard_mode(hmi::text::KeyboardMode mode) noexcept;
  bool toggle_keyboard_language() noexcept;
  bool toggle_shift() noexcept;
  bool backspace() noexcept;
  bool insert_space() noexcept;
  bool complete() noexcept;
  bool edit_review() noexcept;
  bool confirm_review() noexcept;
  void clear_all() noexcept;
  [[nodiscard]] std::string_view field_text(TextFieldId field) const noexcept;

 private:
  using TextId = generated::TextId;

  struct FixedBinding {
    lv_obj_t* label = nullptr;
    TextId id = TextId::page_text_input_title;
  };

  struct CursorBinding {
    TextInputPage* page = nullptr;
    std::uint8_t field_index = 0U;
  };

  static constexpr std::size_t kFieldCount = 3U;
  static constexpr std::size_t kMaxFixedBindings = 40U;

  static void handle_event(lv_event_t* event);
  static void handle_keyboard_event(
      void* context, const hmi::ui::VirtualKeyEvent& event) noexcept;
  static void handle_cursor_selection(void* context,
                                      std::size_t display_index) noexcept;
  static lv_font_t* load_font(void* context, const char* relative_path,
                              std::uint32_t size_bytes,
                              std::uint32_t crc32) noexcept;
  static void free_font(void* context, lv_font_t* font) noexcept;

  lv_obj_t* create_fixed_label(lv_obj_t* parent, TextId id,
                               hmi::ui::TextRole role) noexcept;
  lv_obj_t* create_fixed_button(lv_obj_t* parent, TextId id,
                                hmi::ui::ButtonVariant variant) noexcept;
  bool bind_fixed(lv_obj_t* label, TextId id) noexcept;
  bool create_layout(lv_obj_t* parent) noexcept;
  bool create_font_recovery_layout(lv_obj_t* parent) noexcept;
  bool create_fields(lv_obj_t* parent) noexcept;
  bool create_keyboard(lv_obj_t* parent) noexcept;
  bool create_review_dialog(lv_obj_t* parent) noexcept;
  bool refresh_keyboard() noexcept;
  void refresh_review() noexcept;
  void refresh_locale() noexcept;
  void refresh_fields() noexcept;
  void refresh_field(std::size_t index) noexcept;
  void refresh_status() noexcept;
  void set_edit_status(hmi::text::EditStatus status) noexcept;
  void begin_editing() noexcept;
  bool show_review() noexcept;
  bool move_cursor_from_touch(std::size_t field_index,
                              std::size_t display_index) noexcept;
  [[nodiscard]] std::size_t active_index() const noexcept;
  [[nodiscard]] hmi::text::InputModel& active_model() noexcept;
  [[nodiscard]] const hmi::text::InputModel& active_model() const noexcept;

  PageContext context_{};
  hmi::ui::FontRepository fonts_{};
  std::array<hmi::text::InputModel, kFieldCount> models_{};
  std::array<hmi::ui::UserTextInputLine, kFieldCount> input_lines_{};
  std::array<hmi::ui::UserTextLabel, kFieldCount> review_value_labels_{};
  std::array<CursorBinding, kFieldCount> cursor_bindings_{};
  hmi::ui::VirtualKeyboard keyboard_{};
  std::array<lv_obj_t*, kFieldCount> field_buttons_{};
  std::array<lv_obj_t*, kFieldCount> placeholder_labels_{};
  std::array<lv_obj_t*, kFieldCount> review_empty_labels_{};
  std::array<FixedBinding, kMaxFixedBindings> fixed_bindings_{};
  std::size_t fixed_binding_count_ = 0U;
  TextFieldId active_field_ = TextFieldId::name;
  hmi::text::KeyboardMode last_letter_mode_ = hmi::text::KeyboardMode::hangul;
  TextInputFlowState flow_state_ = TextInputFlowState::editing;
  TextId error_text_ = TextId::text_input_error_character_limit;
  bool has_error_ = false;
  bool font_assets_ready_ = false;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* back_button_ = nullptr;
  lv_obj_t* theme_button_ = nullptr;
  lv_obj_t* locale_ko_button_ = nullptr;
  lv_obj_t* locale_en_button_ = nullptr;
  lv_obj_t* clear_button_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
  lv_obj_t* composing_label_ = nullptr;
  lv_obj_t* error_label_ = nullptr;
  lv_obj_t* confirmed_label_ = nullptr;
  lv_obj_t* review_scrim_ = nullptr;
  lv_obj_t* review_edit_button_ = nullptr;
  lv_obj_t* review_confirm_button_ = nullptr;
};

static_assert(sizeof(TextInputPage) <= 4096U,
              "TextInputPage must keep bounded page state outside callback stacks");

}  // namespace hmi::samples::suite

