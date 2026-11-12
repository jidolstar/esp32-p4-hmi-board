#include "pages/wifi_ap_web/wifi_ap_web_page.hpp"

#include <array>
#include <cstdio>
#include <cstring>

#include <src/extra/libs/qrcode/lv_qrcode.h>

#include "components/overlay_text.hpp"
#include "components/page_header.hpp"
#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "hmi/ui/components.hpp"
#include "pages/wifi_ap_web/generated/web_bundle.hpp"
#include "pages/wifi_ap_web/wifi_ap_web_qr.hpp"

namespace hmi::samples::suite {
namespace {

void apply_fixed_font(lv_obj_t* label, const generated::TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

lv_obj_t* create_label(lv_obj_t* parent, const hmi::i18n::Locale locale,
                       const generated::TextId id, hmi::ui::UiTheme& theme,
                       const hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(locale, id).data(), theme, role);
  apply_fixed_font(label, id);
  return label;
}

lv_obj_t* create_button(lv_obj_t* parent, const hmi::i18n::Locale locale,
                        const generated::TextId id, hmi::ui::UiTheme& theme,
                        const hmi::ui::ButtonVariant variant,
                        const lv_event_cb_t callback, void* user_data) noexcept {
  lv_obj_t* button = hmi::ui::create_button(
      parent, generated::text(locale, id).data(), theme, variant, callback,
      user_data);
  if (button != nullptr) {
    apply_fixed_font(lv_obj_get_child(button, 0), id);
  }
  return button;
}

lv_obj_t* create_panel(lv_obj_t* parent, hmi::ui::UiTheme& theme,
                       const lv_coord_t width) noexcept {
  lv_obj_t* panel = lv_obj_create(parent);
  if (panel == nullptr) {
    return nullptr;
  }
  theme.apply_card(panel);
  lv_obj_set_width(panel, width);
  lv_obj_set_height(panel, LV_PCT(100));
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(panel, 16, 0);
  lv_obj_set_style_pad_row(panel, 9, 0);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  if (width == 0) {
    lv_obj_set_flex_grow(panel, 1);
  }
  return panel;
}

generated::TextId working_status(
    const hmi::connectivity::WifiApRuntimeState state) noexcept {
  using hmi::connectivity::WifiApRuntimeState;
  switch (state) {
    case WifiApRuntimeState::preparing:
      return generated::TextId::wifi_ap_status_preparing;
    case WifiApRuntimeState::starting:
      return generated::TextId::wifi_ap_status_starting;
    case WifiApRuntimeState::stopping:
      return generated::TextId::wifi_ap_status_stopping;
    case WifiApRuntimeState::restoring_sta:
      return generated::TextId::wifi_ap_status_restoring;
    default:
      return generated::TextId::wifi_ap_status_starting;
  }
}

generated::TextId error_status(
    const hmi::connectivity::WifiApRuntimeError error) noexcept {
  using hmi::connectivity::WifiApRuntimeError;
  switch (error) {
    case WifiApRuntimeError::invalid_config:
      return generated::TextId::wifi_ap_error_invalid;
    case WifiApRuntimeError::radio_busy:
      return generated::TextId::wifi_ap_error_radio_busy;
    case WifiApRuntimeError::start_timeout:
    case WifiApRuntimeError::stop_timeout:
      return generated::TextId::wifi_ap_error_timeout;
    case WifiApRuntimeError::restore_failed:
      return generated::TextId::wifi_ap_error_restore;
    case WifiApRuntimeError::start_failed:
    case WifiApRuntimeError::none:
      return generated::TextId::wifi_ap_error_start;
  }
  return generated::TextId::wifi_ap_error_start;
}

}  // namespace

bool WifiApWebPage::create(lv_obj_t* parent, const PageContext& context) {
  if (root_ != nullptr || parent == nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.ap_control == nullptr) {
    return false;
  }
  if (controller_ != nullptr && controller_ != context.ap_control) {
    return false;
  }
  controller_ = context.ap_control;
  if (!controller_->set_web_bundle(wifi_ap_web::generated::kBundle,
                                   wifi_ap_web::generated::kBundleSize)) {
    return false;
  }
  context_ = context;
  hmi::ui::UiTheme& theme = *context_.theme;

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    destroy();
    return false;
  }
  configure_page_shell_root(root_, theme);

  PageHeaderSpec header_spec;
  header_spec.title = generated::TextId::page_wifi_ap_web_title;
  header_spec.locale = context_.locale;
  header_spec.back_event = &WifiApWebPage::handle_back;
  header_spec.locale_ko_event = &WifiApWebPage::handle_locale_ko;
  header_spec.locale_en_event = &WifiApWebPage::handle_locale_en;
  header_spec.theme_event = &WifiApWebPage::handle_theme;
  header_spec.user_data = this;
  if (!create_page_header(root_, header_spec, theme)) {
    destroy();
    return false;
  }

  body_ = lv_obj_create(root_);
  if (body_ == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(body_);
  lv_obj_set_width(body_, LV_PCT(100));
  lv_obj_set_flex_grow(body_, 1);
  lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(body_, 0, 0);

  visual_state_ = VisualState::invalid;
  if (!rebuild_body()) {
    destroy();
    return false;
  }
  timer_ = lv_timer_create(&WifiApWebPage::handle_timer, 100U, this);
  if (timer_ == nullptr) {
    destroy();
    return false;
  }
  return true;
}

WifiApWebPage::VisualState WifiApWebPage::resolve_visual_state() noexcept {
  if (controller_ == nullptr) {
    return VisualState::error;
  }
  const auto snapshot = controller_->snapshot();
  using hmi::connectivity::WifiApRuntimeState;
  switch (snapshot.runtime.state) {
    case WifiApRuntimeState::off:
      return VisualState::off;
    case WifiApRuntimeState::preparing:
    case WifiApRuntimeState::starting:
    case WifiApRuntimeState::stopping:
    case WifiApRuntimeState::restoring_sta:
      return VisualState::working;
    case WifiApRuntimeState::failed:
      return VisualState::error;
    case WifiApRuntimeState::ready:
      break;
  }
  if (snapshot.authenticated_client_count > 0U) {
    ever_web_connected_ = true;
    return VisualState::controlling;
  }
  if (snapshot.runtime.client_count == 0U) {
    return VisualState::waiting_wifi;
  }
  return ever_web_connected_ ? VisualState::reconnecting
                             : VisualState::waiting_web;
}

bool WifiApWebPage::rebuild_body() noexcept {
  if (body_ == nullptr || context_.theme == nullptr) {
    return false;
  }
  lv_obj_clean(body_);
  status_label_ = nullptr;
  message_label_ = nullptr;
  qr_ = nullptr;
  for (std::size_t index = 0U;
       index < hmi::connectivity::kWifiApWebSwitchCount; ++index) {
    switch_cards_[index] = nullptr;
    switch_value_labels_[index] = nullptr;
  }
  visual_state_ = resolve_visual_state();
  if (visual_state_ == VisualState::off ||
      visual_state_ == VisualState::working ||
      visual_state_ == VisualState::error) {
    return build_onboarding(visual_state_);
  }
  return build_active(visual_state_);
}

bool WifiApWebPage::build_onboarding(const VisualState state) noexcept {
  hmi::ui::UiTheme& theme = *context_.theme;
  lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_t* card = lv_obj_create(body_);
  if (card == nullptr) {
    return false;
  }
  theme.apply_card(card);
  lv_obj_set_size(card, 720, 390);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(card, 28, 0);
  lv_obj_set_style_pad_row(card, 14, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* badge = create_label(
      card, context_.locale, generated::TextId::wifi_ap_probe_badge, theme,
      hmi::ui::TextRole::kAccent);
  generated::TextId status_id = generated::TextId::wifi_ap_status_off;
  generated::TextId message_id = generated::TextId::wifi_ap_message_off;
  generated::TextId action_id = generated::TextId::wifi_ap_action_start;
  if (state == VisualState::working) {
    status_id = working_status(controller_->snapshot().runtime.state);
    message_id = generated::TextId::wifi_ap_message_working;
  } else if (state == VisualState::error) {
    status_id = error_status(controller_->snapshot().runtime.error);
    message_id = generated::TextId::wifi_ap_message_failed;
    action_id = generated::TextId::wifi_ap_action_retry;
  }
  status_label_ = create_label(card, context_.locale, status_id, theme,
                               hmi::ui::TextRole::kDisplay);
  message_label_ = create_label(card, context_.locale, message_id, theme,
                                hmi::ui::TextRole::kBody);
  lv_obj_t* note = create_label(
      card, context_.locale, generated::TextId::wifi_ap_probe_note, theme,
      hmi::ui::TextRole::kMuted);
  if (badge == nullptr || status_label_ == nullptr || message_label_ == nullptr ||
      note == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(message_label_, 650, 68, 6,
                               LV_TEXT_ALIGN_CENTER);
  hmi::ui::configure_multiline(note, 650, 70, 6, LV_TEXT_ALIGN_CENTER);
  if (state != VisualState::working) {
    lv_obj_t* action = create_button(
        card, context_.locale, action_id, theme,
        hmi::ui::ButtonVariant::kPrimary, &WifiApWebPage::handle_primary, this);
    if (action == nullptr) {
      return false;
    }
    lv_obj_set_size(action, 220, 56);
  } else {
    lv_obj_t* spinner = lv_spinner_create(card, 900U, 70U);
    if (spinner == nullptr) {
      return false;
    }
    lv_obj_set_size(spinner, 48, 48);
    lv_obj_set_style_arc_color(spinner, theme.accent_color(), LV_PART_INDICATOR);
  }
  return true;
}

bool WifiApWebPage::build_active(const VisualState state) noexcept {
  hmi::ui::UiTheme& theme = *context_.theme;
  lv_obj_set_flex_flow(body_, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(body_, 12, 0);
  lv_obj_t* connection = create_panel(body_, theme, 360);
  lv_obj_t* controls = create_panel(body_, theme, 0);
  if (connection == nullptr || controls == nullptr) {
    return false;
  }

  generated::TextId status_id = generated::TextId::wifi_ap_status_waiting_wifi;
  generated::TextId message_id = generated::TextId::wifi_ap_message_ready;
  if (state == VisualState::waiting_web) {
    status_id = generated::TextId::wifi_ap_status_waiting_web;
    message_id = generated::TextId::wifi_ap_message_web;
  } else if (state == VisualState::controlling) {
    status_id = generated::TextId::wifi_ap_status_connected;
    message_id = generated::TextId::wifi_ap_message_control;
  } else if (state == VisualState::reconnecting) {
    status_id = generated::TextId::wifi_ap_status_reconnecting;
    message_id = generated::TextId::wifi_ap_message_reconnecting;
  }
  status_label_ = create_label(connection, context_.locale, status_id, theme,
                               hmi::ui::TextRole::kTitle);
  message_label_ = create_label(connection, context_.locale, message_id, theme,
                                hmi::ui::TextRole::kBody);
  if (status_label_ == nullptr || message_label_ == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(message_label_, 328, 48, 5,
                               LV_TEXT_ALIGN_LEFT);

  const bool wifi_qr = state == VisualState::waiting_wifi || show_wifi_qr_;
  std::array<char, wifi_ap_web::kMaximumQrPayloadBytes + 1U> payload{};
  std::array<char, hmi::connectivity::kMaximumSsidBytes + 1U> ssid{};
  std::array<char, hmi::connectivity::kMaximumWifiApPasswordBytes + 1U>
      password{};
  std::array<char, 16U> ipv4{};
  std::array<char, hmi::connectivity::kWifiApWebTokenHexBytes + 1U> token{};
  controller_->copy_ssid(ssid.data(), ssid.size());
  controller_->copy_password(password.data(), password.size());
  controller_->copy_ipv4(ipv4.data(), ipv4.size());
  controller_->copy_session_token(token.data(), token.size());
  const bool payload_ready =
      wifi_qr ? wifi_ap_web::make_wifi_qr_payload(
                    ssid.data(), password.data(), payload.data(), payload.size())
              : wifi_ap_web::make_control_url_qr_payload(
                    ipv4.data(), token.data(), payload.data(), payload.size());
  if (payload_ready) {
    lv_obj_t* qr_frame = lv_obj_create(connection);
    if (qr_frame != nullptr) {
      lv_obj_remove_style_all(qr_frame);
      lv_obj_set_size(qr_frame, 220, 220);
      lv_obj_set_style_bg_color(qr_frame, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(qr_frame, LV_OPA_COVER, 0);
      lv_obj_set_style_pad_all(qr_frame, 12, 0);
      lv_obj_clear_flag(qr_frame, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_flex_flow(qr_frame, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(qr_frame, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      qr_ =
          lv_qrcode_create(qr_frame, 196, lv_color_black(), lv_color_white());
    }
    if (qr_ == nullptr ||
        lv_qrcode_update(qr_, payload.data(), std::strlen(payload.data())) !=
            LV_RES_OK) {
      if (qr_frame != nullptr) {
        lv_obj_del(qr_frame);
      }
      qr_ = nullptr;
    }
  }

  const auto create_access_row = [&](const generated::TextId label_id,
                                     const char* value,
                                     const generated::TextId value_font_id) {
    lv_obj_t* row = lv_obj_create(connection);
    if (row == nullptr) {
      return false;
    }
    theme.apply_transparent(row);
    lv_obj_set_size(row, LV_PCT(100), 26);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t* label = create_label(row, context_.locale, label_id, theme,
                                   hmi::ui::TextRole::kMuted);
    lv_obj_t* value_label = hmi::ui::create_label(
        row, value, theme, hmi::ui::TextRole::kBody);
    if (label == nullptr || value_label == nullptr) {
      return false;
    }
    (void)value_font_id;
    // session마다 달라지는 ASCII credential은 catalog glyph 집합으로 제한하지 않는다.
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_width(label, 122);
    lv_obj_set_flex_grow(value_label, 1);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);
    return true;
  };
  if (wifi_qr) {
    if (!create_access_row(generated::TextId::wifi_ap_label_ssid, ssid.data(),
                           generated::TextId::wifi_ap_probe_value_ssid) ||
        !create_access_row(generated::TextId::wifi_ap_label_password,
                           password.data(),
                           generated::TextId::wifi_ap_probe_value_password)) {
      return false;
    }
  } else {
    std::array<char, 32U> url{};
    std::snprintf(url.data(), url.size(), "http://%s/", ipv4.data());
    if (!create_access_row(generated::TextId::wifi_ap_label_endpoint,
                           url.data(),
                           generated::TextId::wifi_ap_probe_value_endpoint)) {
      return false;
    }
    lv_obj_t* note = create_label(
        connection, context_.locale,
        generated::TextId::wifi_ap_message_no_internet, theme,
        hmi::ui::TextRole::kMuted);
    if (note == nullptr) {
      return false;
    }
    hmi::ui::configure_multiline(note, 328, 44, 5, LV_TEXT_ALIGN_LEFT);
  }

  lv_obj_t* control_title = create_label(
      controls, context_.locale,
      state == VisualState::controlling
          ? generated::TextId::wifi_ap_status_connected
          : generated::TextId::page_wifi_ap_web_title,
      theme, hmi::ui::TextRole::kTitle);
  lv_obj_t* control_message = create_label(
      controls, context_.locale, generated::TextId::wifi_ap_message_control,
      theme, hmi::ui::TextRole::kMuted);
  if (control_title == nullptr || control_message == nullptr) {
    return false;
  }
  lv_obj_t* grid = lv_obj_create(controls);
  if (grid == nullptr) {
    return false;
  }
  theme.apply_transparent(grid);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_row(grid, 10, 0);
  lv_obj_set_style_pad_column(grid, 10, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);

  constexpr generated::TextId kSwitchText[] = {
      generated::TextId::wifi_ap_switch_one,
      generated::TextId::wifi_ap_switch_two,
      generated::TextId::wifi_ap_switch_three,
      generated::TextId::wifi_ap_switch_four,
  };
  for (std::size_t index = 0U;
       index < hmi::connectivity::kWifiApWebSwitchCount; ++index) {
    lv_obj_t* card = lv_obj_create(grid);
    if (card == nullptr) {
      return false;
    }
    theme.apply_card(card);
    lv_obj_set_size(card, LV_PCT(48), 132);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(card, &WifiApWebPage::handle_switch,
                        LV_EVENT_SHORT_CLICKED, this);
    lv_obj_t* name = create_label(card, context_.locale, kSwitchText[index],
                                  theme, hmi::ui::TextRole::kTitle);
    switch_value_labels_[index] = create_label(
        card, context_.locale, generated::TextId::wifi_ap_switch_off, theme,
        hmi::ui::TextRole::kMuted);
    if (name == nullptr || switch_value_labels_[index] == nullptr) {
      return false;
    }
    switch_cards_[index] = card;
  }

  lv_obj_t* actions = lv_obj_create(controls);
  if (actions == nullptr) {
    return false;
  }
  theme.apply_transparent(actions);
  lv_obj_set_size(actions, LV_PCT(100), 56);
  lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(actions, 0, 0);
  lv_obj_set_style_pad_column(actions, 10, 0);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  if (state != VisualState::waiting_wifi || show_wifi_qr_) {
    const generated::TextId secondary_id =
        show_wifi_qr_ ? generated::TextId::wifi_ap_action_show_web
                      : generated::TextId::wifi_ap_action_show_wifi;
    lv_obj_t* secondary = create_button(
        actions, context_.locale, secondary_id, theme,
        hmi::ui::ButtonVariant::kSecondary,
        &WifiApWebPage::handle_secondary, this);
    if (secondary == nullptr) {
      return false;
    }
    lv_obj_set_size(secondary, 210, 52);
  }
  lv_obj_t* stop = create_button(
      actions, context_.locale, generated::TextId::wifi_ap_action_stop, theme,
      hmi::ui::ButtonVariant::kSecondary, &WifiApWebPage::handle_stop, this);
  if (stop == nullptr) {
    return false;
  }
  lv_obj_set_size(stop, 150, 52);
  lv_obj_set_style_border_color(stop, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_set_style_border_width(stop, 2, 0);
  update_switches();
  return true;
}

void WifiApWebPage::update_switches() noexcept {
  if (controller_ == nullptr || context_.theme == nullptr) {
    return;
  }
  const auto snapshot = controller_->snapshot();
  for (std::size_t index = 0U;
       index < hmi::connectivity::kWifiApWebSwitchCount; ++index) {
    if (switch_cards_[index] == nullptr ||
        switch_value_labels_[index] == nullptr) {
      continue;
    }
    const bool on = snapshot.switch_values[index];
    const generated::TextId value_id = on
                                           ? generated::TextId::wifi_ap_switch_on
                                           : generated::TextId::wifi_ap_switch_off;
    lv_label_set_text(switch_value_labels_[index],
                      generated::text(context_.locale, value_id).data());
    apply_fixed_font(switch_value_labels_[index], value_id);
    lv_obj_set_style_border_width(switch_cards_[index], on ? 4 : 1, 0);
    lv_obj_set_style_border_color(switch_cards_[index],
                                  context_.theme->accent_color(), 0);
    lv_obj_set_style_text_color(
        switch_value_labels_[index],
        on ? context_.theme->accent_color() : context_.theme->text_color(), 0);
  }
}

void WifiApWebPage::show_stop_confirmation(const bool leave_after_stop) noexcept {
  if (confirmation_ || !root_ || !context_.theme) return;
  using TextId = generated::TextId;
  confirmation_ = hmi::ui::show_confirmation(root_, *context_.theme, {
      overlay_text(context_.locale, TextId::wifi_ap_dialog_stop_title),
      overlay_text(context_.locale, TextId::wifi_ap_dialog_stop_body),
      overlay_text(context_.locale, TextId::wifi_ap_action_cancel),
      overlay_text(context_.locale, TextId::wifi_ap_action_confirm_stop),
      &WifiApWebPage::handle_confirm_cancel, &WifiApWebPage::handle_confirm_stop, this});
  if (confirmation_) leave_on_confirm_ = leave_after_stop;
}

void WifiApWebPage::close_confirmation() noexcept { confirmation_.close(); }

void WifiApWebPage::handle_back(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
  if (self->controller_->snapshot().runtime.state ==
      hmi::connectivity::WifiApRuntimeState::off) {
    self->context_.request_dashboard(self->context_.user_data);
  } else {
    self->show_stop_confirmation(true);
  }
}

void WifiApWebPage::handle_primary(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
  if (self->controller_->snapshot().runtime.state ==
      hmi::connectivity::WifiApRuntimeState::failed) {
    self->controller_->shutdown(lv_tick_get());
  }
  self->controller_->start_session(lv_tick_get());
  self->show_wifi_qr_ = false;
  self->ever_web_connected_ = false;
  self->rebuild_body();
}

void WifiApWebPage::handle_secondary(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
  self->show_wifi_qr_ = !self->show_wifi_qr_;
  self->rebuild_body();
}

void WifiApWebPage::handle_stop(lv_event_t* event) {
  if (event != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    static_cast<WifiApWebPage*>(lv_event_get_user_data(event))
        ->show_stop_confirmation(false);
  }
}

void WifiApWebPage::handle_switch(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
  lv_obj_t* target = lv_event_get_target(event);
  const auto snapshot = self->controller_->snapshot();
  for (std::size_t index = 0U;
       index < hmi::connectivity::kWifiApWebSwitchCount; ++index) {
    if (self->switch_cards_[index] == target) {
      const auto result = self->controller_->apply(
          {snapshot.revision, static_cast<std::uint8_t>(index + 1U),
           !snapshot.switch_values[index]});
      (void)result;
      self->update_switches();
      return;
    }
  }
}

void WifiApWebPage::handle_locale_ko(lv_event_t* event) {
  if (event != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::ko_kr);
  }
}

void WifiApWebPage::handle_locale_en(lv_event_t* event) {
  if (event != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::en_us);
  }
}

void WifiApWebPage::handle_theme(lv_event_t* event) {
  if (event != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
    self->context_.request_theme_toggle(self->context_.user_data);
  }
}

void WifiApWebPage::handle_confirm_cancel(lv_event_t* event) {
  if (event != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    static_cast<WifiApWebPage*>(lv_event_get_user_data(event))
        ->close_confirmation();
  }
}

void WifiApWebPage::handle_confirm_stop(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<WifiApWebPage*>(lv_event_get_user_data(event));
  self->leave_after_stop_ = self->leave_on_confirm_;
  self->close_confirmation();
  self->controller_->stop(lv_tick_get());
  self->rebuild_body();
}

void WifiApWebPage::handle_timer(lv_timer_t* timer) noexcept {
  auto* self = timer == nullptr
                   ? nullptr
                   : static_cast<WifiApWebPage*>(timer->user_data);
  if (self == nullptr || self->controller_ == nullptr) {
    return;
  }
  const VisualState next = self->resolve_visual_state();
  if (next != self->visual_state_) {
    self->rebuild_body();
  } else {
    self->update_switches();
  }
  if (self->leave_after_stop_ &&
      self->controller_->snapshot().runtime.state ==
          hmi::connectivity::WifiApRuntimeState::off) {
    self->leave_after_stop_ = false;
    self->context_.request_dashboard(self->context_.user_data);
  }
}

void WifiApWebPage::destroy() noexcept {
  if (timer_ != nullptr) {
    lv_timer_del(timer_);
  }
  timer_ = nullptr;
  // confirm layer는 page root가 아니라 screen 소유이므로 root보다 먼저 직접 정리한다.
  close_confirmation();
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  body_ = nullptr;
  status_label_ = nullptr;
  message_label_ = nullptr;
  qr_ = nullptr;
  for (std::size_t index = 0U;
       index < hmi::connectivity::kWifiApWebSwitchCount; ++index) {
    switch_cards_[index] = nullptr;
    switch_value_labels_[index] = nullptr;
  }
  visual_state_ = VisualState::invalid;
  context_ = {};
}

WifiApWebPage::~WifiApWebPage() {
  if (controller_ != nullptr) {
    controller_->shutdown(lv_tick_get());
  }
  destroy();
}

}  // namespace hmi::samples::suite
