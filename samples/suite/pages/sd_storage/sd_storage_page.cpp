#include "pages/sd_storage/sd_storage_page.hpp"

#include <algorithm>
#include <cstdio>

#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {

using TextId = generated::TextId;
using State = hmi::storage::StorageAccessState;

void apply_fixed_font(lv_obj_t* label, TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

lv_obj_t* create_label(lv_obj_t* parent, hmi::i18n::Locale locale,
                       TextId id, hmi::ui::UiTheme& theme,
                       hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(locale, id).data(), theme, role);
  apply_fixed_font(label, id);
  return label;
}

lv_obj_t* create_button(lv_obj_t* parent, hmi::i18n::Locale locale,
                        TextId id, hmi::ui::UiTheme& theme,
                        hmi::ui::ButtonVariant variant, lv_event_cb_t callback,
                        void* user_data) noexcept {
  lv_obj_t* button = hmi::ui::create_button(
      parent, generated::text(locale, id).data(), theme, variant, callback,
      user_data);
  if (button != nullptr) {
    apply_fixed_font(lv_obj_get_child(button, 0), id);
  }
  return button;
}

lv_obj_t* create_info_row(lv_obj_t* parent, hmi::i18n::Locale locale,
                          TextId label_id, hmi::ui::UiTheme& theme,
                          lv_obj_t** value_out) noexcept {
  lv_obj_t* row = lv_obj_create(parent);
  if (row == nullptr) {
    return nullptr;
  }
  theme.apply_transparent(row);
  lv_obj_set_size(row, LV_PCT(100), 42);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* label = create_label(row, locale, label_id, theme,
                                 hmi::ui::TextRole::kMuted);
  lv_obj_t* value = create_label(row, locale, TextId::storage_value_unknown,
                                 theme, hmi::ui::TextRole::kTitle);
  if (label == nullptr || value == nullptr) {
    lv_obj_del(row);
    return nullptr;
  }
  *value_out = value;
  return row;
}

TextId state_text(State state) noexcept {
  switch (state) {
    case State::hmi_ready:
      return TextId::storage_status_ready;
    case State::unavailable:
      return TextId::storage_status_unavailable;
    case State::quiescing:
    case State::exposing:
      return TextId::storage_status_connecting;
    case State::waiting_for_host:
      return TextId::storage_status_waiting;
    case State::pc_active:
      return TextId::storage_status_pc_active;
    case State::reclaiming:
      return TextId::storage_status_reclaiming;
    case State::remount_failed:
      return TextId::storage_status_failed;
  }
  return TextId::storage_status_failed;
}

TextId message_text(const hmi::storage::StorageAccessSnapshot& value) noexcept {
  if (value.error == hmi::storage::StorageAccessError::busy) {
    return TextId::storage_message_busy;
  }
  if (value.error != hmi::storage::StorageAccessError::none) {
    return TextId::storage_message_error;
  }
  if (value.state == State::hmi_ready) {
    return value.rescan_revision == 0U ? TextId::storage_message_ready
                                      : TextId::storage_message_returned;
  }
  if (value.state == State::unavailable) {
    return TextId::storage_message_no_media;
  }
  return TextId::storage_message_locked;
}

void set_catalog_text(lv_obj_t* label, hmi::i18n::Locale locale,
                      TextId id) noexcept {
  if (label != nullptr) {
    lv_label_set_text(label, generated::text(locale, id).data());
    apply_fixed_font(label, id);
  }
}

void set_capacity(lv_obj_t* label, std::uint64_t bytes) noexcept {
  if (label == nullptr) {
    return;
  }
  constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
  char buffer[24]{};
  std::snprintf(buffer, sizeof(buffer), "%.1f GiB",
                static_cast<double>(bytes) / kGiB);
  lv_label_set_text(label, buffer);
}

}  // namespace

bool SdStoragePage::create(lv_obj_t* parent, const PageContext& context) {
  if (root_ != nullptr || parent == nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.storage_access == nullptr) {
    return false;
  }
  context_ = context;
  auto& theme = *context_.theme;
  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    destroy();
    return false;
  }
  theme.apply_screen(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(root_, 16, 0);
  lv_obj_set_style_pad_row(root_, 12, 0);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* header = lv_obj_create(root_);
  if (header == nullptr) {
    destroy();
    return false;
  }
  theme.apply_header(header);
  lv_obj_set_size(header, LV_PCT(100), 68);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(header, 10, 0);
  lv_obj_set_style_pad_column(header, 10, 0);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  back_button_ = create_button(header, context_.locale, TextId::common_back,
                               theme, hmi::ui::ButtonVariant::kGhost,
                               &SdStoragePage::handle_event, this);
  lv_obj_t* title = create_label(header, context_.locale,
                                 TextId::page_sd_storage_title, theme,
                                 hmi::ui::TextRole::kDisplay);
  lv_obj_t* spacer = lv_obj_create(header);
  lv_obj_t* ko = create_button(
      header, context_.locale, TextId::locale_ko, theme,
      context_.locale == hmi::i18n::Locale::ko_kr
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &SdStoragePage::handle_locale_ko_event, this);
  lv_obj_t* en = create_button(
      header, context_.locale, TextId::locale_en, theme,
      context_.locale == hmi::i18n::Locale::en_us
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &SdStoragePage::handle_locale_en_event, this);
  const TextId theme_id = theme.mode() == hmi::ui::ThemeMode::kDark
                              ? TextId::theme_light
                              : TextId::theme_dark;
  lv_obj_t* theme_button = create_button(
      header, context_.locale, theme_id, theme,
      hmi::ui::ButtonVariant::kGhost, &SdStoragePage::handle_theme_event, this);
  if (back_button_ == nullptr || title == nullptr || spacer == nullptr ||
      ko == nullptr || en == nullptr || theme_button == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(back_button_, 86, 44);
  theme.apply_transparent(spacer);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_size(ko, 88, 42);
  lv_obj_set_size(en, 88, 42);
  lv_obj_set_size(theme_button, 104, 42);

  lv_obj_t* body = lv_obj_create(root_);
  if (body == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(body);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_style_pad_column(body, 14, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

  lv_obj_t* status_card = lv_obj_create(body);
  lv_obj_t* capacity_card = lv_obj_create(body);
  if (status_card == nullptr || capacity_card == nullptr) {
    destroy();
    return false;
  }
  theme.apply_card(status_card);
  theme.apply_card(capacity_card);
  lv_obj_set_size(status_card, 420, LV_PCT(100));
  lv_obj_set_width(capacity_card, 0);
  lv_obj_set_height(capacity_card, LV_PCT(100));
  lv_obj_set_flex_grow(capacity_card, 1);
  for (lv_obj_t* panel : {status_card, capacity_card}) {
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_set_style_pad_row(panel, 12, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  }

  lv_obj_t* status_title = create_label(status_card, context_.locale,
                                        TextId::storage_card_status, theme,
                                        hmi::ui::TextRole::kTitle);
  lv_obj_t* status_row = create_info_row(status_card, context_.locale,
                                         TextId::storage_label_status, theme,
                                         &status_value_);
  lv_obj_t* owner_row = create_info_row(status_card, context_.locale,
                                        TextId::storage_label_owner, theme,
                                        &owner_value_);
  lv_obj_t* fs_row = create_info_row(status_card, context_.locale,
                                     TextId::storage_label_filesystem, theme,
                                     &filesystem_value_);
  message_ = create_label(status_card, context_.locale,
                          TextId::storage_message_ready, theme,
                          hmi::ui::TextRole::kMuted);
  connect_button_ = create_button(
      status_card, context_.locale, TextId::storage_action_connect, theme,
      hmi::ui::ButtonVariant::kPrimary, &SdStoragePage::handle_event, this);
  if (status_title == nullptr || status_row == nullptr || owner_row == nullptr ||
      fs_row == nullptr || message_ == nullptr || connect_button_ == nullptr) {
    destroy();
    return false;
  }
  hmi::ui::configure_multiline(message_, 360, 78, 6);
  lv_obj_set_flex_grow(message_, 1);
  lv_obj_set_size(connect_button_, LV_PCT(100), 56);

  lv_obj_t* capacity_title = create_label(
      capacity_card, context_.locale, TextId::storage_card_capacity, theme,
      hmi::ui::TextRole::kTitle);
  lv_obj_t* total_row = create_info_row(capacity_card, context_.locale,
                                        TextId::storage_label_total, theme,
                                        &total_value_);
  lv_obj_t* used_row = create_info_row(capacity_card, context_.locale,
                                       TextId::storage_label_used, theme,
                                       &used_value_);
  lv_obj_t* free_row = create_info_row(capacity_card, context_.locale,
                                       TextId::storage_label_free, theme,
                                       &free_value_);
  usage_bar_ = lv_bar_create(capacity_card);
  lv_obj_t* note = create_label(capacity_card, context_.locale,
                                TextId::storage_usb_note, theme,
                                hmi::ui::TextRole::kMuted);
  if (capacity_title == nullptr || total_row == nullptr || used_row == nullptr ||
      free_row == nullptr || usage_bar_ == nullptr || note == nullptr) {
    destroy();
    return false;
  }
  lv_bar_set_range(usage_bar_, 0, 100);
  lv_obj_set_size(usage_bar_, LV_PCT(100), 24);
  lv_obj_set_style_radius(usage_bar_, 12, LV_PART_MAIN);
  lv_obj_set_style_radius(usage_bar_, 12, LV_PART_INDICATOR);
  hmi::ui::configure_multiline(note, 470, 90, 6);
  lv_obj_set_flex_grow(note, 1);

  timer_ = lv_timer_create(&SdStoragePage::handle_timer, 100U, this);
  if (timer_ == nullptr) {
    destroy();
    return false;
  }
  refresh();
  return true;
}

void SdStoragePage::refresh() noexcept {
  if (context_.storage_access != nullptr) {
    update_values(context_.storage_access->snapshot());
  }
}

void SdStoragePage::update_values(
    const hmi::storage::StorageAccessSnapshot& value) noexcept {
  snapshot_.state = value.state;
  snapshot_.error = value.error;
  snapshot_.owner = value.owner;
  snapshot_.generation = value.generation;
  snapshot_.rescan_revision = value.rescan_revision;
  snapshot_.media_present = value.media_present;
  snapshot_.connect_enabled = value.state == State::hmi_ready &&
                              value.media_present && value.filesystem_mounted;
  const std::uint64_t used = value.total_bytes >= value.free_bytes
                                 ? value.total_bytes - value.free_bytes
                                 : 0U;
  snapshot_.used_percent =
      value.total_bytes == 0U
          ? 0U
          : static_cast<std::uint8_t>(std::min<std::uint64_t>(
                100U, (used * 100U) / value.total_bytes));

  set_catalog_text(status_value_, context_.locale, state_text(value.state));
  set_catalog_text(owner_value_, context_.locale,
                   value.owner == hmi::storage::StorageOwner::hmi
                       ? TextId::storage_owner_hmi
                       : (value.owner == hmi::storage::StorageOwner::usb_msc
                              ? TextId::storage_owner_pc
                              : TextId::storage_value_unknown));
  set_catalog_text(filesystem_value_, context_.locale,
                   value.filesystem_mounted ? TextId::storage_filesystem_fat
                                            : TextId::storage_value_unknown);
  set_catalog_text(message_, context_.locale, message_text(value));
  if (value.media_present && value.total_bytes > 0U) {
    set_capacity(total_value_, value.total_bytes);
    set_capacity(used_value_, used);
    set_capacity(free_value_, value.free_bytes);
  } else {
    set_catalog_text(total_value_, context_.locale, TextId::storage_value_unknown);
    set_catalog_text(used_value_, context_.locale, TextId::storage_value_unknown);
    set_catalog_text(free_value_, context_.locale, TextId::storage_value_unknown);
  }
  if (usage_bar_ != nullptr) {
    lv_bar_set_value(usage_bar_, snapshot_.used_percent, LV_ANIM_OFF);
  }
  if (connect_button_ != nullptr) {
    const bool can_refresh = value.state == State::unavailable;
    set_catalog_text(lv_obj_get_child(connect_button_, 0), context_.locale,
                     can_refresh ? TextId::storage_action_refresh
                                 : TextId::storage_action_connect);
    if (snapshot_.connect_enabled || can_refresh) {
      lv_obj_clear_state(connect_button_, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(connect_button_, LV_STATE_DISABLED);
    }
  }
}

bool SdStoragePage::retry_media() noexcept {
  if (context_.storage_access == nullptr ||
      snapshot_.state != State::unavailable) {
    return false;
  }
  const bool accepted = context_.storage_access->retry_media(lv_tick_get());
  refresh();
  return accepted;
}

bool SdStoragePage::request_pc_access() noexcept {
  if (context_.storage_access == nullptr || !snapshot_.connect_enabled) {
    return false;
  }
  const bool accepted = context_.storage_access->request_pc_access(lv_tick_get());
  refresh();
  if (accepted && context_.refresh_storage_lock != nullptr) {
    context_.refresh_storage_lock(context_.user_data);
  }
  return accepted;
}

void SdStoragePage::handle_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<SdStoragePage*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  lv_obj_t* target = lv_event_get_target(event);
  if (target == self->back_button_) {
    self->context_.request_dashboard(self->context_.user_data);
  } else if (target == self->connect_button_) {
    if (self->snapshot_.state == State::unavailable) {
      self->retry_media();
    } else {
      self->request_pc_access();
    }
  }
}

void SdStoragePage::handle_locale_ko_event(lv_event_t* event) {
  auto* self = event == nullptr
                   ? nullptr
                   : static_cast<SdStoragePage*>(lv_event_get_user_data(event));
  if (self != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::ko_kr);
  }
}

void SdStoragePage::handle_locale_en_event(lv_event_t* event) {
  auto* self = event == nullptr
                   ? nullptr
                   : static_cast<SdStoragePage*>(lv_event_get_user_data(event));
  if (self != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::en_us);
  }
}

void SdStoragePage::handle_theme_event(lv_event_t* event) {
  auto* self = event == nullptr
                   ? nullptr
                   : static_cast<SdStoragePage*>(lv_event_get_user_data(event));
  if (self != nullptr && lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    self->context_.request_theme_toggle(self->context_.user_data);
  }
}

void SdStoragePage::handle_timer(lv_timer_t* timer) noexcept {
  auto* self = timer == nullptr
                   ? nullptr
                   : static_cast<SdStoragePage*>(timer->user_data);
  if (self != nullptr) {
    self->refresh();
  }
}

SdStoragePageSnapshot SdStoragePage::storage_snapshot() const noexcept {
  return snapshot_;
}

void SdStoragePage::destroy() noexcept {
  if (timer_ != nullptr) {
    lv_timer_del(timer_);
  }
  timer_ = nullptr;
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  back_button_ = nullptr;
  connect_button_ = nullptr;
  status_value_ = nullptr;
  owner_value_ = nullptr;
  filesystem_value_ = nullptr;
  total_value_ = nullptr;
  used_value_ = nullptr;
  free_value_ = nullptr;
  usage_bar_ = nullptr;
  message_ = nullptr;
  context_ = {};
}

SdStoragePage::~SdStoragePage() { destroy(); }

}  // namespace hmi::samples::suite
