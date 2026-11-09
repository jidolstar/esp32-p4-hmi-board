#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <lvgl.h>

#include "hmi/connectivity/wifi_sta_registration.hpp"
#include "hmi/text/input_model.hpp"
#include "hmi/ui/font_repository.hpp"
#include "hmi/ui/user_text_input_line.hpp"
#include "hmi/ui/user_text_label.hpp"
#include "hmi/ui/virtual_keyboard.hpp"
#include "page_contract.hpp"
#include "generated/text_catalog.hpp"

namespace hmi::samples::suite {

enum class WifiInputField : std::uint8_t { ssid, password };
enum class WifiPageView : std::uint8_t { list, editor };

struct WifiPageSnapshot {
  hmi::i18n::Locale locale = hmi::i18n::Locale::ko_kr;
  hmi::text::KeyboardMode keyboard_mode = hmi::text::KeyboardMode::latin;
  WifiPageView view = WifiPageView::list;
  WifiInputField active_field = WifiInputField::ssid;
  hmi::connectivity::WifiRegistrationState registration_state =
      hmi::connectivity::WifiRegistrationState::idle;
  hmi::connectivity::WifiRegistrationError registration_error =
      hmi::connectivity::WifiRegistrationError::none;
  std::uint32_t scan_generation = 0U;
  std::uint8_t scan_count = 0U;
  std::uint8_t signal_level = 0U;
  std::uint8_t selected_index = 0U;
  bool has_selection = false;
  bool has_ip = false;
  bool operation_owned = false;
  bool scan_pending = false;
};

/**
 * 검색 목록과 자격 증명 입력을 단계별 화면으로 분리하는 Wi-Fi 설정 page다.
 *
 * registration은 host가 소유한다. page는 자신이 시작한 미완료 작업만 이탈 시 취소하며
 * 이미 저장·연결된 STA는 유지한다. password model은 표시나 snapshot에 노출하지 않고
 * 연결 요청 직후와 destroy에서 명시적으로 비운다.
 */
class WifiSettingsPage final : public SamplePage {
 public:
  WifiSettingsPage() = default;
  WifiSettingsPage(const WifiSettingsPage&) = delete;
  WifiSettingsPage& operator=(const WifiSettingsPage&) = delete;
  ~WifiSettingsPage() override;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override { return {}; }
  bool handle_utf8_input(const char* utf8) noexcept override;
  bool handle_key(hmi::profiles::AppKey key) noexcept override;

  bool request_scan() noexcept;
  bool select_network(std::size_t index) noexcept;
  bool show_manual_entry() noexcept;
  bool cancel_edit() noexcept;
  bool select_field(WifiInputField field) noexcept;
  bool connect() noexcept;
  bool disconnect() noexcept;
  bool retry_save() noexcept;
  bool refresh() noexcept;
  bool set_locale(hmi::i18n::Locale locale) noexcept;
  bool set_keyboard_mode(hmi::text::KeyboardMode mode) noexcept;
  bool press_character(std::size_t index) noexcept;
  bool backspace() noexcept;
  [[nodiscard]] WifiPageSnapshot wifi_snapshot() const noexcept;
  [[nodiscard]] std::string_view field_text(WifiInputField field) const noexcept;

 private:
  using TextId = generated::TextId;

  struct FixedBinding {
    lv_obj_t* label = nullptr;
    TextId id = TextId::page_wifi_title;
  };

  struct NetworkRow {
    lv_obj_t* button = nullptr;
    lv_obj_t* meta_label = nullptr;
    lv_obj_t* badge = nullptr;
    hmi::ui::UserTextLabel ssid_label{};
  };

  static constexpr std::size_t kFieldCount = 2U;
  static constexpr std::size_t kMaxFixedBindings = 48U;

  static void handle_event(lv_event_t* event);
  static void handle_timer(lv_timer_t* timer) noexcept;
  static void handle_keyboard_event(
      void* context, const hmi::ui::VirtualKeyEvent& event) noexcept;
  static lv_font_t* load_font(void* context, const char* relative_path,
                              std::uint32_t size_bytes,
                              std::uint32_t crc32) noexcept;
  static void free_font(void* context, lv_font_t* font) noexcept;

  bool create_layout(lv_obj_t* parent) noexcept;
  bool create_fields(lv_obj_t* parent) noexcept;
  bool create_keyboard(lv_obj_t* parent) noexcept;
  bool create_status_panel(lv_obj_t* parent) noexcept;
  bool rebuild_network_rows(std::size_t count) noexcept;
  bool bind_fixed(lv_obj_t* label, TextId id) noexcept;
  lv_obj_t* create_fixed_label(lv_obj_t* parent, TextId id,
                               hmi::ui::TextRole role) noexcept;
  lv_obj_t* create_fixed_button(lv_obj_t* parent, TextId id,
                                hmi::ui::ButtonVariant variant) noexcept;
  bool render_keyboard() noexcept;
  void refresh_locale() noexcept;
  void refresh_fields() noexcept;
  void refresh_networks() noexcept;
  void refresh_status() noexcept;
  void refresh_view() noexcept;
  void service_pending_scan() noexcept;
  void clear_password() noexcept;
  [[nodiscard]] std::size_t active_index() const noexcept;
  [[nodiscard]] hmi::text::InputModel& active_model() noexcept;
  [[nodiscard]] const hmi::text::InputModel& active_model() const noexcept;

  PageContext context_{};
  hmi::ui::FontRepository fonts_{};
  std::array<hmi::text::InputModel, kFieldCount> models_{};
  std::array<hmi::ui::UserTextInputLine, kFieldCount> input_lines_{};
  hmi::ui::VirtualKeyboard keyboard_{};
  std::unique_ptr<NetworkRow[]> network_rows_{};
  std::size_t network_row_count_ = 0U;
  hmi::ui::UserTextLabel editor_ssid_label_{};
  hmi::ui::UserTextLabel detail_label_{};
  std::array<FixedBinding, kMaxFixedBindings> fixed_bindings_{};
  std::size_t fixed_binding_count_ = 0U;
  WifiInputField active_field_ = WifiInputField::ssid;
  WifiPageView view_ = WifiPageView::list;
  hmi::text::KeyboardMode last_letter_mode_ = hmi::text::KeyboardMode::latin;
  std::uint32_t visible_generation_ = 0U;
  std::uint8_t visible_count_ = 0U;
  std::uint8_t selected_index_ = 0U;
  bool has_selection_ = false;
  bool operation_owned_ = false;
  bool scan_pending_ = false;
  bool input_initialized_ = false;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* back_button_ = nullptr;
  lv_obj_t* theme_button_ = nullptr;
  lv_obj_t* locale_ko_button_ = nullptr;
  lv_obj_t* locale_en_button_ = nullptr;
  lv_obj_t* refresh_button_ = nullptr;
  lv_obj_t* manual_button_ = nullptr;
  lv_obj_t* editor_cancel_button_ = nullptr;
  lv_obj_t* list_view_ = nullptr;
  lv_obj_t* editor_view_ = nullptr;
  lv_obj_t* fields_row_ = nullptr;
  lv_obj_t* editor_heading_label_ = nullptr;
  lv_obj_t* editor_status_label_ = nullptr;
  lv_obj_t* scan_state_panel_ = nullptr;
  lv_obj_t* scan_spinner_ = nullptr;
  lv_obj_t* scan_message_label_ = nullptr;
  lv_obj_t* network_rows_container_ = nullptr;
  lv_obj_t* ssid_field_ = nullptr;
  lv_obj_t* password_field_ = nullptr;
  lv_obj_t* connect_button_ = nullptr;
  lv_obj_t* disconnect_button_ = nullptr;
  lv_obj_t* retry_button_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
  lv_timer_t* timer_ = nullptr;
};

static_assert(sizeof(WifiSettingsPage) <= 4096U,
              "Wi-Fi page must keep bounded state outside callback stacks");

}  // namespace hmi::samples::suite
