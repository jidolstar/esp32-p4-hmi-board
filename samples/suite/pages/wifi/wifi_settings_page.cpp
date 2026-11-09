#include "pages/wifi/wifi_settings_page.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <new>

#include "hmi/i18n/format.hpp"
#include "hmi/text/utf8.hpp"
#include "hmi/ui/components.hpp"
#include "generated/fixed_fonts.hpp"
#include "generated/user_font_manifest.hpp"

namespace hmi::samples::suite {
namespace {

std::size_t field_index(const WifiInputField field) noexcept {
  return static_cast<std::size_t>(field);
}

constexpr lv_coord_t kNetworkRowHeight = 78;
constexpr lv_coord_t kNetworkRowGap = 8;

generated::TextId status_text(
    const hmi::connectivity::WifiRegistrationSnapshot& snapshot) noexcept {
  using Error = hmi::connectivity::WifiRegistrationError;
  using State = hmi::connectivity::WifiRegistrationState;
  if (snapshot.error != Error::none) {
    switch (snapshot.error) {
      case Error::busy:
        return generated::TextId::wifi_error_busy;
      case Error::invalid_credential:
        return generated::TextId::wifi_error_invalid_credential;
      case Error::unsupported_security:
        return generated::TextId::wifi_error_unsupported_security;
      case Error::stale_selection:
        return generated::TextId::wifi_error_stale_selection;
      case Error::no_access_point:
        return generated::TextId::wifi_error_no_access_point;
      case Error::authentication_failed:
        return generated::TextId::wifi_error_authentication;
      case Error::timeout:
        return generated::TextId::wifi_error_timeout;
      case Error::storage_load:
      case Error::storage_save:
      case Error::storage_erase:
        return generated::TextId::wifi_error_storage;
      case Error::restore_failed:
        return generated::TextId::wifi_error_restore;
      case Error::driver_start:
        return generated::TextId::wifi_error_driver;
      case Error::none:
        break;
    }
  }
  switch (snapshot.state) {
    case State::idle:
      return generated::TextId::wifi_status_idle;
    case State::scanning:
      return generated::TextId::wifi_status_scanning;
    case State::connecting:
      return generated::TextId::wifi_status_connecting;
    case State::saved:
      return generated::TextId::wifi_status_connected;
    case State::connected_not_saved:
      return generated::TextId::wifi_status_connected_not_saved;
    case State::restoring:
      return generated::TextId::wifi_status_restoring;
    case State::failed:
      return generated::TextId::wifi_status_failed;
    case State::cancelled:
      return generated::TextId::wifi_status_cancelled;
    case State::forgotten:
      return generated::TextId::wifi_status_forgotten;
  }
  return generated::TextId::wifi_status_idle;
}

}  // namespace

WifiSettingsPage::~WifiSettingsPage() { destroy(); }

bool WifiSettingsPage::create(lv_obj_t* parent, const PageContext& context) {
  if (parent == nullptr || root_ != nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.wifi_registration == nullptr || context.fonts.load == nullptr ||
      context.fonts.free == nullptr) {
    return false;
  }
  context_ = context;
  if (!input_initialized_) {
    (void)models_[0].set_mode(hmi::text::KeyboardMode::latin);
    (void)models_[1].set_mode(hmi::text::KeyboardMode::latin);
    input_initialized_ = true;
  }
  if (!fonts_.initialize(generated::kUserFontManifest,
                         {this, &WifiSettingsPage::load_font,
                          &WifiSettingsPage::free_font}) ||
      fonts_.resolve(static_cast<std::uint32_t>('A')).font == nullptr) {
    context_ = {};
    fonts_.reset();
    return false;
  }
  if (!create_layout(parent)) {
    destroy();
    return false;
  }
  timer_ = lv_timer_create(&WifiSettingsPage::handle_timer, 150U, this);
  if (timer_ == nullptr) {
    destroy();
    return false;
  }
  refresh_locale();
  refresh_fields();
  scan_pending_ =
      context_.wifi_registration->scan_snapshot().state !=
      hmi::connectivity::WifiScanState::ready;
  return refresh();
}

lv_font_t* WifiSettingsPage::load_font(void* context,
                                       const char* relative_path,
                                       const std::uint32_t size_bytes,
                                       const std::uint32_t crc32) noexcept {
  auto* self = static_cast<WifiSettingsPage*>(context);
  if (self == nullptr || self->context_.fonts.load == nullptr) {
    return nullptr;
  }
  return static_cast<lv_font_t*>(self->context_.fonts.load(
      self->context_.fonts.context, relative_path, size_bytes, crc32));
}

void WifiSettingsPage::free_font(void* context, lv_font_t* font) noexcept {
  auto* self = static_cast<WifiSettingsPage*>(context);
  if (self != nullptr && self->context_.fonts.free != nullptr) {
    self->context_.fonts.free(self->context_.fonts.context, font);
  }
}

bool WifiSettingsPage::bind_fixed(lv_obj_t* label, const TextId id) noexcept {
  if (label == nullptr || fixed_binding_count_ >= fixed_bindings_.size()) {
    return false;
  }
  fixed_bindings_[fixed_binding_count_++] = {label, id};
  return true;
}

lv_obj_t* WifiSettingsPage::create_fixed_label(
    lv_obj_t* parent, const TextId id, const hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(context_.locale, id).data(), *context_.theme, role);
  if (label == nullptr || !bind_fixed(label, id)) {
    if (label != nullptr) {
      lv_obj_del(label);
    }
    return nullptr;
  }
  lv_obj_set_style_text_font(label,
                             generated::fixed_font(generated::typography(id)), 0);
  return label;
}

lv_obj_t* WifiSettingsPage::create_fixed_button(
    lv_obj_t* parent, const TextId id,
    const hmi::ui::ButtonVariant variant) noexcept {
  lv_obj_t* button = hmi::ui::create_button(
      parent, generated::text(context_.locale, id).data(), *context_.theme,
      variant, &WifiSettingsPage::handle_event, this);
  if (button == nullptr) {
    return nullptr;
  }
  // 공통 button helper의 전체 event 구독을 제거하고 실제 손을 뗀 클릭만 한 번
  // 처리한다. 짧은 탭 판정 시간에 걸린 터치가 무시되는 문제도 함께 피한다.
  (void)lv_obj_remove_event_cb(button, &WifiSettingsPage::handle_event);
  lv_obj_add_event_cb(button, &WifiSettingsPage::handle_event,
                      LV_EVENT_CLICKED, this);
  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label == nullptr || !bind_fixed(label, id)) {
    lv_obj_del(button);
    return nullptr;
  }
  lv_obj_set_style_text_font(label,
                             generated::fixed_font(generated::typography(id)), 0);
  return button;
}

bool WifiSettingsPage::create_layout(lv_obj_t* parent) noexcept {
  auto& theme = *context_.theme;
  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  theme.apply_screen(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(root_, 14, 0);
  lv_obj_set_style_pad_row(root_, 12, 0);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* header = lv_obj_create(root_);
  if (header == nullptr) {
    return false;
  }
  theme.apply_header(header);
  lv_obj_set_size(header, LV_PCT(100), 62);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(header, 8, 0);
  lv_obj_set_style_pad_column(header, 10, 0);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  back_button_ = create_fixed_button(
      header, TextId::common_back, hmi::ui::ButtonVariant::kGhost);
  lv_obj_t* title = create_fixed_label(
      header, TextId::page_wifi_title, hmi::ui::TextRole::kTitle);
  lv_obj_t* spacer = lv_obj_create(header);
  locale_ko_button_ = create_fixed_button(
      header, TextId::locale_ko,
      context_.locale == hmi::i18n::Locale::ko_kr
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost);
  locale_en_button_ = create_fixed_button(
      header, TextId::locale_en,
      context_.locale == hmi::i18n::Locale::en_us
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost);
  theme_button_ = create_fixed_button(
      header,
      theme.mode() == hmi::ui::ThemeMode::kDark ? TextId::theme_light
                                                 : TextId::theme_dark,
      hmi::ui::ButtonVariant::kGhost);
  if (back_button_ == nullptr || title == nullptr || spacer == nullptr ||
      locale_ko_button_ == nullptr || locale_en_button_ == nullptr ||
      theme_button_ == nullptr) {
    return false;
  }
  lv_obj_set_size(back_button_, 78, 44);
  theme.apply_transparent(spacer);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_size(locale_ko_button_, 92, 44);
  lv_obj_set_size(locale_en_button_, 92, 44);
  lv_obj_set_size(theme_button_, 110, 44);

  lv_obj_t* body = lv_obj_create(root_);
  if (body == nullptr) {
    return false;
  }
  theme.apply_transparent(body);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(body, 0, 0);

  list_view_ = lv_obj_create(body);
  editor_view_ = lv_obj_create(body);
  if (list_view_ == nullptr || editor_view_ == nullptr) {
    return false;
  }
  theme.apply_transparent(list_view_);
  lv_obj_set_size(list_view_, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(list_view_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(list_view_, 0, 0);
  lv_obj_set_style_pad_column(list_view_, 12, 0);
  lv_obj_set_flex_flow(list_view_, LV_FLEX_FLOW_ROW);

  lv_obj_t* connection = lv_obj_create(list_view_);
  lv_obj_t* networks = lv_obj_create(list_view_);
  if (connection == nullptr || networks == nullptr) {
    return false;
  }
  theme.apply_surface(connection);
  lv_obj_set_size(connection, 340, LV_PCT(100));
  lv_obj_clear_flag(connection, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(connection, 14, 0);
  lv_obj_set_style_pad_row(connection, 10, 0);
  lv_obj_set_flex_flow(connection, LV_FLEX_FLOW_COLUMN);
  if (create_fixed_label(connection, TextId::wifi_connection_title,
                         hmi::ui::TextRole::kTitle) == nullptr ||
      !create_status_panel(connection)) {
    return false;
  }

  theme.apply_surface(networks);
  lv_obj_set_height(networks, LV_PCT(100));
  lv_obj_set_flex_grow(networks, 1);
  lv_obj_clear_flag(networks, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(networks, 12, 0);
  lv_obj_set_style_pad_row(networks, 8, 0);
  lv_obj_set_flex_flow(networks, LV_FLEX_FLOW_COLUMN);
  lv_obj_t* network_header = lv_obj_create(networks);
  if (network_header == nullptr) {
    return false;
  }
  theme.apply_transparent(network_header);
  lv_obj_set_size(network_header, LV_PCT(100), 48);
  lv_obj_clear_flag(network_header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(network_header, 0, 0);
  lv_obj_set_flex_flow(network_header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(network_header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (create_fixed_label(network_header, TextId::wifi_scan_title,
                         hmi::ui::TextRole::kTitle) == nullptr) {
    return false;
  }
  refresh_button_ = create_fixed_button(
      network_header, TextId::wifi_scan_refresh,
      hmi::ui::ButtonVariant::kSecondary);
  if (refresh_button_ == nullptr) {
    return false;
  }
  lv_obj_set_size(refresh_button_, 112, 44);

  lv_obj_t* network_list = lv_obj_create(networks);
  if (network_list == nullptr) {
    return false;
  }
  theme.apply_transparent(network_list);
  lv_obj_set_width(network_list, LV_PCT(100));
  lv_obj_set_flex_grow(network_list, 1);
  lv_obj_set_scroll_dir(network_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(network_list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_pad_all(network_list, 0, 0);
  lv_obj_set_style_pad_row(network_list, 8, 0);
  lv_obj_set_flex_flow(network_list, LV_FLEX_FLOW_COLUMN);

  scan_state_panel_ = lv_obj_create(network_list);
  if (scan_state_panel_ == nullptr) {
    return false;
  }
  theme.apply_transparent(scan_state_panel_);
  lv_obj_set_size(scan_state_panel_, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(scan_state_panel_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(scan_state_panel_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(scan_state_panel_, 0, 0);
  scan_spinner_ = lv_spinner_create(scan_state_panel_, 900, 60);
  scan_message_label_ = hmi::ui::create_label(
      scan_state_panel_, "", theme, hmi::ui::TextRole::kMuted);
  if (scan_spinner_ == nullptr || scan_message_label_ == nullptr) {
    return false;
  }
  lv_obj_set_size(scan_spinner_, 42, 42);
  lv_obj_align(scan_spinner_, LV_ALIGN_CENTER, 0, -28);
  lv_obj_set_style_arc_color(scan_spinner_, theme.accent_color(),
                             LV_PART_INDICATOR);
  hmi::ui::configure_multiline(scan_message_label_, LV_PCT(100), 64, 5,
                               LV_TEXT_ALIGN_CENTER);
  lv_obj_align(scan_message_label_, LV_ALIGN_CENTER, 0, 32);
  network_rows_container_ = lv_obj_create(network_list);
  if (network_rows_container_ == nullptr) {
    return false;
  }
  theme.apply_transparent(network_rows_container_);
  lv_obj_set_size(network_rows_container_, LV_PCT(100), 1);
  lv_obj_clear_flag(network_rows_container_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(network_rows_container_, 0, 0);
  lv_obj_set_style_pad_row(network_rows_container_, 8, 0);
  lv_obj_set_flex_flow(network_rows_container_, LV_FLEX_FLOW_COLUMN);

  manual_button_ = create_fixed_button(
      networks, TextId::wifi_action_manual_entry,
      hmi::ui::ButtonVariant::kSecondary);
  if (manual_button_ == nullptr) {
    return false;
  }
  lv_obj_set_size(manual_button_, LV_PCT(100), 48);

  theme.apply_surface(editor_view_);
  lv_obj_set_size(editor_view_, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(editor_view_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(editor_view_, 14, 0);
  lv_obj_set_style_pad_row(editor_view_, 8, 0);
  lv_obj_set_flex_flow(editor_view_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* selection = lv_obj_create(editor_view_);
  if (selection == nullptr) {
    return false;
  }
  theme.apply_status(selection);
  lv_obj_set_size(selection, LV_PCT(100), 88);
  lv_obj_clear_flag(selection, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(selection, 10, 0);
  editor_heading_label_ = hmi::ui::create_label(
      selection, "", theme, hmi::ui::TextRole::kMuted);
  if (editor_heading_label_ == nullptr ||
      !editor_ssid_label_.create(selection, fonts_)) {
    return false;
  }
  theme.apply_label(editor_ssid_label_.object(), hmi::ui::TextRole::kBody);
  lv_obj_set_width(editor_heading_label_, LV_PCT(55));
  lv_obj_set_pos(editor_heading_label_, 0, 0);
  lv_obj_set_size(editor_ssid_label_.object(), LV_PCT(55), 30);
  lv_obj_set_pos(editor_ssid_label_.object(), 0, 36);
  editor_status_label_ = hmi::ui::create_label(
      selection, "", theme, hmi::ui::TextRole::kAccent);
  if (editor_status_label_ == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(editor_status_label_, LV_PCT(40), 62, 5,
                               LV_TEXT_ALIGN_LEFT);
  lv_obj_align(editor_status_label_, LV_ALIGN_RIGHT_MID, -8, 0);

  return create_fields(editor_view_) && create_keyboard(editor_view_);
}

bool WifiSettingsPage::create_status_panel(lv_obj_t* parent) noexcept {
  lv_obj_t* status = lv_obj_create(parent);
  if (status == nullptr) {
    return false;
  }
  context_.theme->apply_status(status);
  lv_obj_set_width(status, LV_PCT(100));
  lv_obj_set_height(status, 150);
  lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(status, 12, 0);
  status_label_ = hmi::ui::create_label(
      status, "", *context_.theme, hmi::ui::TextRole::kAccent);
  if (status_label_ == nullptr || !detail_label_.create(status, fonts_)) {
    return false;
  }
  context_.theme->apply_label(detail_label_.object(),
                              hmi::ui::TextRole::kBody);
  hmi::ui::configure_multiline(status_label_, LV_PCT(100), 92, 5,
                               LV_TEXT_ALIGN_LEFT);
  lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_width(detail_label_.object(), LV_PCT(100));
  lv_obj_align(detail_label_.object(), LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t* spacer = lv_obj_create(parent);
  if (spacer == nullptr) {
    return false;
  }
  context_.theme->apply_transparent(spacer);
  lv_obj_set_size(spacer, LV_PCT(100), 1);
  lv_obj_set_flex_grow(spacer, 1);

  retry_button_ = create_fixed_button(
      parent, TextId::wifi_action_retry_save,
      hmi::ui::ButtonVariant::kSecondary);
  disconnect_button_ = create_fixed_button(
      parent, TextId::wifi_action_disconnect,
      hmi::ui::ButtonVariant::kGhost);
  if (retry_button_ == nullptr || disconnect_button_ == nullptr) {
    return false;
  }
  lv_obj_set_size(retry_button_, LV_PCT(100), 46);
  lv_obj_set_size(disconnect_button_, LV_PCT(100), 46);
  return true;
}

bool WifiSettingsPage::rebuild_network_rows(const std::size_t count) noexcept {
  if (network_rows_container_ == nullptr ||
      count > hmi::connectivity::kMaximumWifiScanEntries) {
    return false;
  }

  // 검색 결과 수만큼만 widget과 label owner를 만든다. 최대치 32개를 page에
  // 상시 내장하지 않아 idle 화면의 객체 크기와 LVGL heap 사용을 함께 제한한다.
  lv_obj_clean(network_rows_container_);
  network_rows_.reset();
  network_row_count_ = 0U;
  if (count == 0U) {
    lv_obj_set_height(network_rows_container_, 1);
    return true;
  }

  std::unique_ptr<NetworkRow[]> rows{new (std::nothrow) NetworkRow[count]};
  if (!rows) {
    return false;
  }
  for (std::size_t index = 0U; index < count; ++index) {
    NetworkRow& row = rows[index];
    row.button = lv_obj_create(network_rows_container_);
    if (row.button == nullptr) {
      lv_obj_clean(network_rows_container_);
      return false;
    }
    context_.theme->apply_list_item(row.button);
    lv_obj_set_size(row.button, LV_PCT(100), kNetworkRowHeight);
    lv_obj_clear_flag(row.button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row.button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row.button, &WifiSettingsPage::handle_event,
                        LV_EVENT_CLICKED, this);
    lv_obj_t* text_group = lv_obj_create(row.button);
    if (text_group == nullptr) {
      lv_obj_clean(network_rows_container_);
      return false;
    }
    context_.theme->apply_transparent(text_group);
    lv_obj_set_size(text_group, LV_PCT(72), 58);
    lv_obj_align(text_group, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_clear_flag(text_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(text_group, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(text_group, 0, 0);
    if (!row.ssid_label.create(text_group, fonts_)) {
      lv_obj_clean(network_rows_container_);
      return false;
    }
    context_.theme->apply_label(row.ssid_label.object(),
                                hmi::ui::TextRole::kBody);
    lv_obj_set_size(row.ssid_label.object(), LV_PCT(100), 30);
    lv_obj_set_pos(row.ssid_label.object(), 0, 0);
    // UserTextLabel의 투명 root는 기본적으로 clickable이므로 행의 터치를
    // 가로채지 않게 한다. 실제 선택 event는 바깥 row가 받는다.
    lv_obj_clear_flag(row.ssid_label.object(), LV_OBJ_FLAG_CLICKABLE);
    row.meta_label = hmi::ui::create_label(
        text_group, "", *context_.theme, hmi::ui::TextRole::kMuted);
    row.badge = hmi::ui::create_label(
        row.button, "", *context_.theme, hmi::ui::TextRole::kAccent);
    if (row.meta_label == nullptr || row.badge == nullptr) {
      lv_obj_clean(network_rows_container_);
      return false;
    }
    // status font의 실제 line height보다 넉넉한 높이를 보장해 한글 하단과
    // 영문 descender가 행 내부에서 잘리지 않게 한다.
    lv_obj_set_size(row.meta_label, LV_PCT(100), 24);
    lv_obj_set_pos(row.meta_label, 0, 34);
    lv_label_set_long_mode(row.meta_label, LV_LABEL_LONG_DOT);
    lv_obj_align(row.badge, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_add_flag(row.badge, LV_OBJ_FLAG_HIDDEN);
  }
  network_rows_ = std::move(rows);
  network_row_count_ = count;
  lv_obj_set_height(network_rows_container_,
                    static_cast<lv_coord_t>(
                        count * static_cast<std::size_t>(kNetworkRowHeight) +
                        (count - 1U) *
                            static_cast<std::size_t>(kNetworkRowGap)));
  return true;
}

bool WifiSettingsPage::create_fields(lv_obj_t* parent) noexcept {
  fields_row_ = lv_obj_create(parent);
  if (fields_row_ == nullptr) {
    return false;
  }
  context_.theme->apply_transparent(fields_row_);
  lv_obj_set_size(fields_row_, LV_PCT(100), 64);
  lv_obj_clear_flag(fields_row_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(fields_row_, 0, 0);
  lv_obj_set_style_pad_column(fields_row_, 8, 0);
  lv_obj_set_flex_flow(fields_row_, LV_FLEX_FLOW_ROW);

  std::array<lv_obj_t**, kFieldCount> fields = {&ssid_field_, &password_field_};
  constexpr std::array<TextId, kFieldCount> labels = {
      TextId::wifi_field_ssid, TextId::wifi_field_password};
  for (std::size_t index = 0U; index < fields.size(); ++index) {
    *fields[index] = lv_obj_create(fields_row_);
    if (*fields[index] == nullptr) {
      return false;
    }
    context_.theme->apply_card(*fields[index]);
    lv_obj_set_height(*fields[index], LV_PCT(100));
    lv_obj_set_flex_grow(*fields[index], 1);
    lv_obj_clear_flag(*fields[index], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(*fields[index], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(*fields[index], &WifiSettingsPage::handle_event,
                        LV_EVENT_CLICKED, this);
    lv_obj_set_style_pad_all(*fields[index], 8, 0);
    lv_obj_t* label = create_fixed_label(*fields[index], labels[index],
                                         hmi::ui::TextRole::kMuted);
    if (label == nullptr ||
        !input_lines_[index].create(*fields[index], fonts_, 430, 30)) {
      return false;
    }
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, -2);
    lv_obj_align(input_lines_[index].object(), LV_ALIGN_BOTTOM_LEFT, 0, 1);
    input_lines_[index].set_colors(context_.theme->text_color(),
                                   context_.theme->accent_color());
  }

  lv_obj_t* actions = lv_obj_create(parent);
  if (actions == nullptr) {
    return false;
  }
  context_.theme->apply_transparent(actions);
  lv_obj_set_size(actions, LV_PCT(100), 48);
  lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(actions, 0, 0);
  lv_obj_set_style_pad_column(actions, 8, 0);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
  editor_cancel_button_ = create_fixed_button(
      actions, TextId::wifi_action_cancel,
      hmi::ui::ButtonVariant::kGhost);
  connect_button_ = create_fixed_button(
      actions, TextId::wifi_action_connect,
      hmi::ui::ButtonVariant::kPrimary);
  if (editor_cancel_button_ == nullptr || connect_button_ == nullptr) {
    return false;
  }
  lv_obj_set_size(editor_cancel_button_, 180, 46);
  lv_obj_set_size(connect_button_, 180, 46);
  lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return true;
}

bool WifiSettingsPage::create_keyboard(lv_obj_t* parent) noexcept {
  lv_obj_update_layout(parent);
  const lv_coord_t width = lv_obj_get_content_width(parent);
  const lv_coord_t height = static_cast<lv_coord_t>(
      lv_obj_get_content_height(parent) - 88 - 64 - 48 - 24);
  if (!keyboard_.create(
          parent, fonts_, *context_.theme,
          generated::fixed_font(
              generated::typography(TextId::text_input_keyboard_shift)),
          width, height)) {
    return false;
  }
  keyboard_.set_event_handler(&WifiSettingsPage::handle_keyboard_event, this);
  return render_keyboard();
}

bool WifiSettingsPage::render_keyboard() noexcept {
  if (keyboard_.object() == nullptr) {
    return false;
  }
  const hmi::ui::VirtualKeyboardCaptions captions{
      generated::text(context_.locale, TextId::text_input_keyboard_shift).data(),
      generated::text(context_.locale, TextId::text_input_keyboard_symbol).data(),
      generated::text(context_.locale,
                      TextId::text_input_keyboard_language_toggle)
          .data(),
      generated::text(context_.locale, TextId::text_input_keyboard_space).data(),
      generated::text(context_.locale,
                      TextId::text_input_keyboard_backspace)
          .data(),
      generated::text(context_.locale, TextId::wifi_action_connect).data(),
  };
  return keyboard_.render(active_model().mode(), active_model().shift(), captions);
}

void WifiSettingsPage::refresh_locale() noexcept {
  for (std::size_t index = 0U; index < fixed_binding_count_; ++index) {
    const FixedBinding& binding = fixed_bindings_[index];
    lv_label_set_text(binding.label,
                      generated::text(context_.locale, binding.id).data());
    lv_obj_set_style_text_font(
        binding.label, generated::fixed_font(generated::typography(binding.id)), 0);
  }
  (void)render_keyboard();
  refresh_view();
  refresh_networks();
  refresh_status();
}

void WifiSettingsPage::refresh_fields() noexcept {
  for (std::size_t index = 0U; index < models_.size(); ++index) {
    const bool password = index == field_index(WifiInputField::password);
    const auto text = models_[index].editor().text();
    if (password) {
      std::array<char, hmi::connectivity::kMaximumPasswordBytes + 1U> mask{};
      const std::size_t count = models_[index].editor().codepoint_count();
      for (std::size_t cursor = 0U; cursor < count && cursor + 1U < mask.size();
           ++cursor) {
        mask[cursor] = '*';
      }
      (void)input_lines_[index].render(
          std::string_view(mask.data(), count),
          models_[index].editor().cursor_codepoint_index());
    } else {
      (void)input_lines_[index].render(
          text, models_[index].editor().cursor_codepoint_index(),
          models_[index].preview_codepoint());
    }
    input_lines_[index].set_active(index == active_index());
  }
}

void WifiSettingsPage::refresh_networks() noexcept {
  const auto scan = context_.wifi_registration->scan_snapshot();
  const bool rows_current = visible_generation_ == scan.generation &&
                            network_row_count_ == scan.count;
  const bool rows_ready = rows_current || rebuild_network_rows(scan.count);
  visible_generation_ = scan.generation;
  visible_count_ = rows_ready ? scan.count : 0U;
  const bool loading = scan_pending_ ||
                       scan.state == hmi::connectivity::WifiScanState::queued ||
                       scan.state == hmi::connectivity::WifiScanState::scanning;
  const bool show_state = loading || visible_count_ == 0U;
  if (show_state) {
    TextId message_id = TextId::wifi_scan_empty;
    if (!rows_ready) {
      message_id = TextId::wifi_status_failed;
    } else if (loading) {
      message_id = TextId::wifi_status_scanning;
    } else if (scan.state == hmi::connectivity::WifiScanState::failed ||
               scan.state == hmi::connectivity::WifiScanState::timed_out) {
      message_id = TextId::wifi_status_failed;
    }
    lv_label_set_text(scan_message_label_,
                      generated::text(context_.locale, message_id).data());
    lv_obj_set_style_text_font(
        scan_message_label_,
        generated::fixed_font(generated::typography(message_id)), 0);
    lv_obj_clear_flag(scan_state_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(network_rows_container_, LV_OBJ_FLAG_HIDDEN);
    if (loading) {
      lv_obj_clear_flag(scan_spinner_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(scan_spinner_, LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    lv_obj_add_flag(scan_state_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(network_rows_container_, LV_OBJ_FLAG_HIDDEN);
  }

  const auto registration = context_.wifi_registration->snapshot();
  const std::size_t connected_ssid_length =
      std::strlen(registration.current_ssid);
  for (std::size_t index = 0U; index < network_row_count_; ++index) {
    NetworkRow& row = network_rows_[index];
    const auto* entry = context_.wifi_registration->scan_entry(
        scan.generation, index);
    if (entry == nullptr) {
      lv_obj_add_flag(row.button, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    (void)row.ssid_label.set_text(
        std::string_view(entry->ssid, entry->ssid_length));
    TextId meta_id = TextId::wifi_scan_meta_secured;
    if (entry->security == hmi::connectivity::WifiSecurity::open) {
      meta_id = TextId::wifi_scan_meta_open;
    } else if (entry->security ==
               hmi::connectivity::WifiSecurity::unsupported) {
      meta_id = TextId::wifi_scan_meta_unsupported;
    }
    std::array<char, 96U> meta{};
    std::snprintf(meta.data(), meta.size(),
                  generated::text(context_.locale, meta_id).data(),
                  static_cast<int>(entry->rssi));
    lv_label_set_text(row.meta_label, meta.data());
    lv_obj_set_style_text_font(
        row.meta_label,
        generated::fixed_font(generated::typography(meta_id)), 0);

    const bool supported = entry->security !=
                           hmi::connectivity::WifiSecurity::unsupported;
    if (supported) {
      lv_obj_add_flag(row.button, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_clear_flag(row.button, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_opa(row.button,
                         supported ? LV_OPA_COVER : LV_OPA_70, 0);

    const bool connected = registration.has_ip &&
                           entry->ssid_length == connected_ssid_length &&
                           std::memcmp(entry->ssid, registration.current_ssid,
                                       connected_ssid_length) == 0;
    const bool selected = !connected && has_selection_ &&
                          selected_index_ == index;
    const bool emphasized = connected || selected;
    lv_obj_set_style_border_width(row.button, emphasized ? 3 : 1, 0);
    lv_obj_set_style_border_color(row.button,
                                  context_.theme->accent_color(), 0);
    lv_obj_set_style_border_opa(row.button,
                                emphasized ? LV_OPA_COVER : LV_OPA_30, 0);
    if (emphasized) {
      lv_label_set_text(
          row.badge,
          generated::text(context_.locale,
                          connected ? TextId::wifi_scan_connected
                                    : TextId::wifi_scan_selected)
              .data());
      lv_obj_set_style_text_font(
          row.badge,
          generated::fixed_font(
              generated::typography(connected ? TextId::wifi_scan_connected
                                               : TextId::wifi_scan_selected)),
          0);
      lv_obj_clear_flag(row.badge, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(row.badge, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(row.button, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiSettingsPage::refresh_view() noexcept {
  if (list_view_ == nullptr || editor_view_ == nullptr) {
    return;
  }
  if (view_ == WifiPageView::list) {
    lv_obj_clear_flag(list_view_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(editor_view_, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_add_flag(list_view_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(editor_view_, LV_OBJ_FLAG_HIDDEN);
  const TextId heading_id = has_selection_
                                ? TextId::wifi_editor_selected_title
                                : TextId::wifi_editor_manual_title;
  lv_label_set_text(editor_heading_label_,
                    generated::text(context_.locale, heading_id).data());
  lv_obj_set_style_text_font(
      editor_heading_label_,
      generated::fixed_font(generated::typography(heading_id)), 0);
  (void)editor_ssid_label_.set_text(has_selection_
                                        ? models_[0].editor().text()
                                        : generated::text(
                                              context_.locale,
                                              TextId::wifi_status_manual));
  if (has_selection_) {
    lv_obj_add_flag(ssid_field_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(ssid_field_, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiSettingsPage::refresh_status() noexcept {
  if (context_.wifi_registration == nullptr || status_label_ == nullptr) {
    return;
  }
  const auto registration = context_.wifi_registration->snapshot();
  TextId status_id = status_text(registration);
  if (registration.error == hmi::connectivity::WifiRegistrationError::none &&
      registration.state == hmi::connectivity::WifiRegistrationState::idle) {
    if (view_ == WifiPageView::editor) {
      status_id = has_selection_ ? TextId::wifi_status_ready
                                 : TextId::wifi_status_manual;
    } else if (has_selection_) {
      status_id = TextId::wifi_status_selected;
    }
  }
  lv_label_set_text(status_label_,
                    generated::text(context_.locale, status_id).data());
  lv_obj_set_style_text_font(
      status_label_, generated::fixed_font(generated::typography(status_id)), 0);
  if (editor_status_label_ != nullptr) {
    lv_label_set_text(editor_status_label_,
                      generated::text(context_.locale, status_id).data());
    lv_obj_set_style_text_font(
        editor_status_label_,
        generated::fixed_font(generated::typography(status_id)), 0);
  }

  const bool busy = context_.wifi_registration->busy();
  for (lv_obj_t* field : {ssid_field_, password_field_, connect_button_}) {
    if (field == nullptr) {
      continue;
    }
    if (busy) {
      lv_obj_add_state(field, LV_STATE_DISABLED);
    } else {
      lv_obj_clear_state(field, LV_STATE_DISABLED);
    }
  }

  std::array<char, 192U> detail{};
  if (registration.has_ip) {
    const std::array arguments = {
        hmi::i18n::FormatArgument::text("ssid", registration.current_ssid)};
    const auto result = hmi::i18n::format_to(
        generated::text(context_.locale, TextId::wifi_detail_connected),
        arguments.data(), arguments.size(), detail.data(), detail.size());
    if (result.status != hmi::i18n::FormatStatus::ok) {
      detail[0] = '\0';
    }
  } else if (has_selection_) {
    const auto ssid = models_[0].editor().text();
    const std::array arguments = {
        hmi::i18n::FormatArgument::text("ssid", ssid)};
    const auto result = hmi::i18n::format_to(
        generated::text(context_.locale, TextId::wifi_detail_selected),
        arguments.data(), arguments.size(), detail.data(), detail.size());
    if (result.status != hmi::i18n::FormatStatus::ok) {
      detail[0] = '\0';
    }
  }
  (void)detail_label_.set_text(detail.data());
  const bool retry = registration.state ==
                     hmi::connectivity::WifiRegistrationState::connected_not_saved;
  if (retry) {
    lv_obj_clear_flag(retry_button_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(retry_button_, LV_OBJ_FLAG_HIDDEN);
  }
  if (registration.has_ip || registration.has_saved_credential) {
    lv_obj_clear_flag(disconnect_button_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(disconnect_button_, LV_OBJ_FLAG_HIDDEN);
  }
}

bool WifiSettingsPage::refresh() noexcept {
  if (context_.wifi_registration == nullptr || root_ == nullptr) {
    return false;
  }
  service_pending_scan();
  const auto registration = context_.wifi_registration->snapshot();
  if (view_ == WifiPageView::editor && operation_owned_) {
    const bool connection_succeeded =
        registration.has_ip &&
        (registration.state ==
             hmi::connectivity::WifiRegistrationState::saved ||
         registration.state ==
             hmi::connectivity::WifiRegistrationState::connected_not_saved);
    if (connection_succeeded) {
      // 연결 요청 수락이 아니라 실제 link/IP 확인이 끝난 뒤 목록으로 돌아간다.
      view_ = WifiPageView::list;
      active_field_ = WifiInputField::ssid;
    } else if (!context_.wifi_registration->busy() &&
               registration.state ==
                   hmi::connectivity::WifiRegistrationState::failed) {
      // 실패한 password는 이미 지웠다. 같은 SSID를 유지한 채 빈 password
      // field에 focus를 돌려 사용자가 곧바로 다시 입력하게 한다.
      active_field_ = WifiInputField::password;
      refresh_fields();
      (void)render_keyboard();
    }
  }
  refresh_networks();
  refresh_view();
  refresh_status();
  if (operation_owned_ && !context_.wifi_registration->busy()) {
    operation_owned_ = false;
  }
  return true;
}

void WifiSettingsPage::service_pending_scan() noexcept {
  if (!scan_pending_ || context_.wifi_registration == nullptr ||
      context_.wifi_registration->busy()) {
    return;
  }
  if (context_.wifi_registration->request_scan()) {
    scan_pending_ = false;
    operation_owned_ = true;
  }
}

void WifiSettingsPage::handle_timer(lv_timer_t* timer) noexcept {
  auto* self = timer == nullptr
                   ? nullptr
                   : static_cast<WifiSettingsPage*>(timer->user_data);
  if (self != nullptr) {
    (void)self->refresh();
  }
}

void WifiSettingsPage::handle_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  auto* self = static_cast<WifiSettingsPage*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  lv_obj_t* target = lv_event_get_target(event);
  if (target == self->back_button_) {
    if (self->view_ == WifiPageView::editor) {
      self->cancel_edit();
    } else {
      self->context_.request_dashboard(self->context_.user_data);
    }
  } else if (target == self->theme_button_) {
    self->context_.request_theme_toggle(self->context_.user_data);
  } else if (target == self->locale_ko_button_) {
    self->set_locale(hmi::i18n::Locale::ko_kr);
  } else if (target == self->locale_en_button_) {
    self->set_locale(hmi::i18n::Locale::en_us);
  } else if (target == self->refresh_button_) {
    self->request_scan();
  } else if (target == self->manual_button_) {
    self->show_manual_entry();
  } else if (target == self->editor_cancel_button_) {
    self->cancel_edit();
  } else if (target == self->ssid_field_) {
    self->select_field(WifiInputField::ssid);
  } else if (target == self->password_field_) {
    self->select_field(WifiInputField::password);
  } else if (target == self->connect_button_) {
    self->connect();
  } else if (target == self->disconnect_button_) {
    self->disconnect();
  } else if (target == self->retry_button_) {
    self->retry_save();
  } else {
    for (std::size_t index = 0U; index < self->network_row_count_; ++index) {
      if (target == self->network_rows_[index].button) {
        self->select_network(index);
        return;
      }
    }
  }
}

void WifiSettingsPage::handle_keyboard_event(
    void* context, const hmi::ui::VirtualKeyEvent& event) noexcept {
  auto* self = static_cast<WifiSettingsPage*>(context);
  if (self == nullptr || self->view_ != WifiPageView::editor) {
    return;
  }
  switch (event.action) {
    case hmi::ui::VirtualKeyAction::layout_character:
      self->press_character(event.layout_index);
      break;
    case hmi::ui::VirtualKeyAction::literal_character:
      if (event.codepoint >= 0x20U && event.codepoint <= 0x7EU) {
        (void)self->active_model().insert_codepoint(event.codepoint);
        if (self->active_field_ == WifiInputField::ssid) {
          self->has_selection_ = false;
        }
        self->refresh_fields();
      }
      break;
    case hmi::ui::VirtualKeyAction::shift:
      self->active_model().toggle_shift();
      self->render_keyboard();
      break;
    case hmi::ui::VirtualKeyAction::symbol:
      self->set_keyboard_mode(
          self->active_model().mode() == hmi::text::KeyboardMode::symbol
              ? self->last_letter_mode_
              : hmi::text::KeyboardMode::symbol);
      break;
    case hmi::ui::VirtualKeyAction::language:
      self->set_keyboard_mode(
          self->last_letter_mode_ == hmi::text::KeyboardMode::hangul
              ? hmi::text::KeyboardMode::latin
              : hmi::text::KeyboardMode::hangul);
      break;
    case hmi::ui::VirtualKeyAction::space:
      (void)self->active_model().insert_space();
      if (self->active_field_ == WifiInputField::ssid) {
        self->has_selection_ = false;
      }
      self->refresh_fields();
      break;
    case hmi::ui::VirtualKeyAction::backspace:
      self->backspace();
      break;
    case hmi::ui::VirtualKeyAction::submit:
      self->connect();
      break;
  }
}

bool WifiSettingsPage::request_scan() noexcept {
  if (context_.wifi_registration == nullptr) {
    return false;
  }

  const auto scan = context_.wifi_registration->scan_snapshot();
  if (scan.state == hmi::connectivity::WifiScanState::queued ||
      scan.state == hmi::connectivity::WifiScanState::scanning) {
    operation_owned_ = true;
    return true;
  }

  // 저장 자격 증명 자동 연결 중의 새로고침도 버리지 않는다. 202609 화면과
  // 동일하게 연결 상태가 끝난 첫 tick에서 실제 scan 요청으로 전환한다.
  scan_pending_ = true;
  service_pending_scan();
  return refresh();
}

bool WifiSettingsPage::select_network(const std::size_t index) noexcept {
  if (context_.wifi_registration == nullptr || index >= visible_count_) {
    return false;
  }
  const auto* entry = context_.wifi_registration->scan_entry(
      visible_generation_, index);
  if (entry == nullptr ||
      entry->security == hmi::connectivity::WifiSecurity::unsupported ||
      models_[0].set_text(std::string_view(entry->ssid, entry->ssid_length)) !=
          hmi::text::EditStatus::ok) {
    return false;
  }
  selected_index_ = static_cast<std::uint8_t>(index);
  has_selection_ = true;
  view_ = WifiPageView::editor;
  active_field_ = entry->security == hmi::connectivity::WifiSecurity::open
                      ? WifiInputField::ssid
                      : WifiInputField::password;
  (void)active_model().move_cursor_to(active_model().editor().codepoint_count());
  refresh_fields();
  (void)render_keyboard();
  refresh_view();
  refresh_networks();
  refresh_status();
  return true;
}

bool WifiSettingsPage::show_manual_entry() noexcept {
  if (root_ == nullptr || context_.wifi_registration == nullptr ||
      context_.wifi_registration->busy()) {
    return false;
  }
  has_selection_ = false;
  models_[field_index(WifiInputField::ssid)].clear();
  clear_password();
  active_field_ = WifiInputField::ssid;
  view_ = WifiPageView::editor;
  refresh_fields();
  (void)render_keyboard();
  refresh_view();
  refresh_networks();
  refresh_status();
  return true;
}

bool WifiSettingsPage::cancel_edit() noexcept {
  if (root_ == nullptr || view_ != WifiPageView::editor) {
    return false;
  }
  if (operation_owned_ && context_.wifi_registration != nullptr &&
      context_.wifi_registration->busy()) {
    context_.wifi_registration->cancel();
    operation_owned_ = false;
  }
  // 편집 화면에서 뒤로 가기는 연결 후보 선택까지 취소한다. 같은 AP를 다시
  // 누르면 비밀번호 입력 기회를 즉시 다시 얻을 수 있어야 한다.
  has_selection_ = false;
  selected_index_ = 0U;
  models_[field_index(WifiInputField::ssid)].clear();
  clear_password();
  active_field_ = WifiInputField::ssid;
  view_ = WifiPageView::list;
  refresh_fields();
  refresh_view();
  refresh_networks();
  refresh_status();
  return true;
}

bool WifiSettingsPage::select_field(const WifiInputField field) noexcept {
  if (field_index(field) >= models_.size()) {
    return false;
  }
  if (view_ == WifiPageView::list) {
    return field == WifiInputField::ssid && show_manual_entry();
  }
  (void)active_model().complete();
  active_field_ = field;
  (void)active_model().move_cursor_to(active_model().editor().codepoint_count());
  refresh_fields();
  return render_keyboard();
}

bool WifiSettingsPage::connect() noexcept {
  if (context_.wifi_registration == nullptr ||
      view_ != WifiPageView::editor) {
    return false;
  }
  (void)models_[0].complete();
  (void)models_[1].complete();
  const auto ssid = models_[0].editor().text();
  const auto password = models_[1].editor().text();
  bool accepted = false;
  if (has_selection_) {
    accepted = context_.wifi_registration->connect_selected(
        visible_generation_, selected_index_,
        reinterpret_cast<const std::uint8_t*>(password.data()), password.size());
  } else {
    accepted = context_.wifi_registration->connect_hidden(
        reinterpret_cast<const std::uint8_t*>(ssid.data()), ssid.size(),
        reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
        hmi::connectivity::WifiSecurity::personal);
  }
  operation_owned_ = accepted;
  clear_password();
  active_field_ = WifiInputField::password;
  refresh_fields();
  (void)render_keyboard();
  refresh_view();
  refresh_networks();
  refresh_status();
  return accepted;
}

bool WifiSettingsPage::disconnect() noexcept {
  if (context_.wifi_registration == nullptr ||
      !context_.wifi_registration->forget()) {
    return false;
  }
  operation_owned_ = false;
  has_selection_ = false;
  view_ = WifiPageView::list;
  models_[field_index(WifiInputField::ssid)].clear();
  active_field_ = WifiInputField::ssid;
  clear_password();
  refresh_fields();
  return refresh();
}

bool WifiSettingsPage::retry_save() noexcept {
  return context_.wifi_registration != nullptr &&
         context_.wifi_registration->retry_save() && refresh();
}

bool WifiSettingsPage::set_locale(const hmi::i18n::Locale locale) noexcept {
  if (!context_.set_locale(context_.user_data, locale)) {
    return false;
  }
  context_.locale = locale;
  refresh_locale();
  return true;
}

bool WifiSettingsPage::set_keyboard_mode(
    const hmi::text::KeyboardMode mode) noexcept {
  if (mode != hmi::text::KeyboardMode::latin &&
      mode != hmi::text::KeyboardMode::hangul &&
      mode != hmi::text::KeyboardMode::symbol &&
      mode != hmi::text::KeyboardMode::number) {
    return false;
  }
  if (active_model().set_mode(mode) != hmi::text::EditStatus::ok) {
    return false;
  }
  if (mode == hmi::text::KeyboardMode::latin ||
      mode == hmi::text::KeyboardMode::hangul) {
    last_letter_mode_ = mode;
  }
  refresh_fields();
  return render_keyboard();
}

bool WifiSettingsPage::press_character(const std::size_t index) noexcept {
  if (view_ != WifiPageView::editor) {
    return false;
  }
  const auto layout = hmi::text::keyboard_layout(active_model().mode(),
                                                  active_model().shift());
  if (index >= layout.size) {
    return false;
  }
  const bool consume_shift = active_model().shift();
  const auto status = active_model().press_key(index);
  if (status != hmi::text::EditStatus::ok) {
    return false;
  }
  if (consume_shift) {
    active_model().toggle_shift();
    (void)render_keyboard();
  }
  if (active_field_ == WifiInputField::ssid) {
    has_selection_ = false;
  }
  refresh_fields();
  return true;
}

bool WifiSettingsPage::backspace() noexcept {
  if (view_ != WifiPageView::editor) {
    return false;
  }
  if (active_model().backspace() != hmi::text::EditStatus::ok) {
    return false;
  }
  if (active_field_ == WifiInputField::ssid) {
    has_selection_ = false;
  }
  refresh_fields();
  return true;
}

bool WifiSettingsPage::handle_utf8_input(const char* utf8) noexcept {
  if (utf8 == nullptr || view_ != WifiPageView::editor) {
    return false;
  }
  const std::string_view value{utf8};
  for (const unsigned char byte : value) {
    if (byte < 0x20U || byte > 0x7EU) {
      return false;
    }
  }
  if (active_model().complete() != hmi::text::EditStatus::ok) {
    return false;
  }
  for (const unsigned char byte : value) {
    if (active_model().insert_codepoint(byte) != hmi::text::EditStatus::ok) {
      return false;
    }
  }
  if (active_field_ == WifiInputField::ssid) {
    has_selection_ = false;
  }
  refresh_fields();
  return true;
}

bool WifiSettingsPage::handle_key(const hmi::profiles::AppKey key) noexcept {
  if (key == hmi::profiles::AppKey::enter) {
    return view_ == WifiPageView::editor && connect();
  }
  hmi::text::EditStatus status = hmi::text::EditStatus::ok;
  switch (key) {
    case hmi::profiles::AppKey::backspace:
      return backspace();
    case hmi::profiles::AppKey::cursor_left:
      status = active_model().move_cursor(-1);
      break;
    case hmi::profiles::AppKey::cursor_right:
      status = active_model().move_cursor(1);
      break;
    case hmi::profiles::AppKey::enter:
      break;
  }
  refresh_fields();
  return status == hmi::text::EditStatus::ok;
}

WifiPageSnapshot WifiSettingsPage::wifi_snapshot() const noexcept {
  WifiPageSnapshot value;
  value.locale = context_.locale;
  value.keyboard_mode = active_model().mode();
  value.view = view_;
  value.active_field = active_field_;
  value.scan_generation = visible_generation_;
  value.scan_count = visible_count_;
  value.selected_index = selected_index_;
  value.has_selection = has_selection_;
  value.operation_owned = operation_owned_;
  value.scan_pending = scan_pending_;
  if (context_.wifi_registration != nullptr) {
    const auto registration = context_.wifi_registration->snapshot();
    value.registration_state = registration.state;
    value.registration_error = registration.error;
    value.signal_level = registration.signal_level;
    value.has_ip = registration.has_ip;
  }
  return value;
}

std::string_view WifiSettingsPage::field_text(
    const WifiInputField field) const noexcept {
  // 비밀번호는 test/debug API에서도 평문을 반환하지 않는다.
  return field == WifiInputField::ssid ? models_[0].editor().text()
                                       : std::string_view{};
}

std::size_t WifiSettingsPage::active_index() const noexcept {
  return field_index(active_field_);
}

hmi::text::InputModel& WifiSettingsPage::active_model() noexcept {
  return models_[active_index()];
}

const hmi::text::InputModel& WifiSettingsPage::active_model() const noexcept {
  return models_[active_index()];
}

void WifiSettingsPage::clear_password() noexcept {
  // Editor의 logical clear 뒤에도 배열에 남을 수 있는 비밀을 객체 전체에서 지운다.
  auto& password = models_[field_index(WifiInputField::password)];
  password.~InputModel();
  auto* bytes = reinterpret_cast<volatile unsigned char*>(&password);
  for (std::size_t index = 0U; index < sizeof(password); ++index) {
    bytes[index] = 0U;
  }
  new (&password) hmi::text::InputModel();
  (void)password.set_mode(hmi::text::KeyboardMode::latin);
}

void WifiSettingsPage::destroy() noexcept {
  if (timer_ != nullptr) {
    lv_timer_del(timer_);
  }
  timer_ = nullptr;
  if (operation_owned_ && context_.wifi_registration != nullptr) {
    const auto scan = context_.wifi_registration->scan_snapshot();
    if (scan.state == hmi::connectivity::WifiScanState::queued ||
        scan.state == hmi::connectivity::WifiScanState::scanning ||
        scan.draining) {
      context_.wifi_registration->cancel();
    }
  }
  operation_owned_ = false;
  scan_pending_ = false;
  keyboard_.destroy();
  for (auto& line : input_lines_) {
    line.destroy();
  }
  for (std::size_t index = 0U; index < network_row_count_; ++index) {
    network_rows_[index].ssid_label.destroy();
  }
  network_rows_.reset();
  network_row_count_ = 0U;
  editor_ssid_label_.destroy();
  detail_label_.destroy();
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  fonts_.reset();
  clear_password();
  fixed_bindings_.fill({});
  fixed_binding_count_ = 0U;
  view_ = WifiPageView::list;
  active_field_ = WifiInputField::ssid;
  visible_generation_ = 0U;
  visible_count_ = 0U;
  selected_index_ = 0U;
  has_selection_ = false;
  models_[field_index(WifiInputField::ssid)].clear();
  back_button_ = nullptr;
  theme_button_ = nullptr;
  locale_ko_button_ = nullptr;
  locale_en_button_ = nullptr;
  refresh_button_ = nullptr;
  manual_button_ = nullptr;
  editor_cancel_button_ = nullptr;
  list_view_ = nullptr;
  editor_view_ = nullptr;
  fields_row_ = nullptr;
  editor_heading_label_ = nullptr;
  editor_status_label_ = nullptr;
  scan_state_panel_ = nullptr;
  scan_spinner_ = nullptr;
  scan_message_label_ = nullptr;
  network_rows_container_ = nullptr;
  ssid_field_ = nullptr;
  password_field_ = nullptr;
  connect_button_ = nullptr;
  disconnect_button_ = nullptr;
  retry_button_ = nullptr;
  status_label_ = nullptr;
  context_ = {};
}

}  // namespace hmi::samples::suite
