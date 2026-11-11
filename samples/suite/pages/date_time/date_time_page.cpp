#include "pages/date_time/date_time_page.hpp"

#include <array>
#include <cstdio>

#include "components/page_header.hpp"
#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {

using TextId = generated::TextId;

constexpr std::array<DateTimeField, 6U> kFields = {
    DateTimeField::year, DateTimeField::month, DateTimeField::day,
    DateTimeField::hour, DateTimeField::minute, DateTimeField::second};

void apply_fixed_font(lv_obj_t* label, const TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

TextId source_text(const hmi::time::TimeSource source) noexcept {
  switch (source) {
    case hmi::time::TimeSource::manual:
      return TextId::date_time_source_manual;
    case hmi::time::TimeSource::rtc:
      return TextId::date_time_source_rtc;
    case hmi::time::TimeSource::network:
      return TextId::date_time_source_network;
    case hmi::time::TimeSource::unset:
      return TextId::date_time_source_unset;
  }
  return TextId::date_time_source_unset;
}

TextId rtc_text(const hmi::time::RtcStatus status) noexcept {
  switch (status) {
    case hmi::time::RtcStatus::ready:
      return TextId::date_time_rtc_ready;
    case hmi::time::RtcStatus::invalid:
      return TextId::date_time_rtc_invalid;
    case hmi::time::RtcStatus::not_detected:
      return TextId::date_time_rtc_unsupported;
    case hmi::time::RtcStatus::read_error:
      return TextId::date_time_rtc_read_error;
    case hmi::time::RtcStatus::write_error:
      return TextId::date_time_rtc_write_error;
  }
  return TextId::date_time_rtc_unsupported;
}

TextId network_text(const hmi::time::NetworkState state) noexcept {
  switch (state) {
    case hmi::time::NetworkState::idle:
      return TextId::date_time_network_idle;
    case hmi::time::NetworkState::waiting_for_ip:
      return TextId::date_time_network_waiting;
    case hmi::time::NetworkState::syncing:
      return TextId::date_time_network_syncing;
    case hmi::time::NetworkState::synced:
      return TextId::date_time_network_synced;
    case hmi::time::NetworkState::retry_waiting:
      return TextId::date_time_network_retry;
  }
  return TextId::date_time_network_idle;
}

TextId apply_text(const DateTimePageSnapshot& state) noexcept {
  if (!state.has_apply_result) {
    return TextId::date_time_apply_ready;
  }
  switch (state.last_apply) {
    case hmi::time::ManualApplyResult::applied:
      return TextId::date_time_apply_success;
    case hmi::time::ManualApplyResult::applied_rtc_write_failed:
      return TextId::date_time_apply_partial;
    case hmi::time::ManualApplyResult::invalid_value:
      return TextId::date_time_apply_invalid;
    case hmi::time::ManualApplyResult::system_write_failed:
      return TextId::date_time_apply_system_error;
  }
  return TextId::date_time_apply_invalid;
}

void configure_container(lv_obj_t* object, hmi::ui::UiTheme& theme,
                         const bool card) noexcept {
  if (card) {
    theme.apply_card(object);
  } else {
    theme.apply_transparent(object);
  }
  lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* create_summary_group(lv_obj_t* parent, hmi::ui::UiTheme& theme,
                               const std::uint8_t flex_grow) noexcept {
  lv_obj_t* group = lv_obj_create(parent);
  if (group == nullptr) {
    return nullptr;
  }
  // 서로 관련된 제목과 값만 한 구획에 담아 상태 항목의 경계를 분명하게 보인다.
  theme.apply_surface(group);
  lv_obj_set_size(group, LV_PCT(100), 0);
  lv_obj_set_flex_grow(group, flex_grow);
  lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(group, 10, 0);
  lv_obj_set_style_pad_all(group, 8, 0);
  lv_obj_set_style_pad_row(group, 3, 0);
  lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
  return group;
}

}  // namespace

DateTimePage::~DateTimePage() { destroy(); }

bool DateTimePage::create(lv_obj_t* parent, const PageContext& context) {
  if (parent == nullptr || root_ != nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.time_runtime == nullptr || !context.time_runtime->valid()) {
    return false;
  }
  context_ = context;
  refresh_current();
  reset_draft();
  editor_tick_ms_ = lv_tick_get();
  editor_remainder_ms_ = 0U;

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    destroy();
    return false;
  }
  configure_page_shell_root(root_, *context.theme);

  if (!create_header(root_) || !create_body(root_) || !create_footer(root_)) {
    destroy();
    return false;
  }
  timer_ = lv_timer_create(&DateTimePage::handle_timer, 200U, this);
  if (timer_ == nullptr) {
    destroy();
    return false;
  }
  refresh();
  return true;
}

lv_obj_t* DateTimePage::create_label(lv_obj_t* parent, const TextId id,
                                     const hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(context_.locale, id).data(), *context_.theme, role);
  apply_fixed_font(label, id);
  return label;
}

lv_obj_t* DateTimePage::create_button(
    lv_obj_t* parent, const TextId id,
    const hmi::ui::ButtonVariant variant) noexcept {
  lv_obj_t* button = hmi::ui::create_button(
      parent, generated::text(context_.locale, id).data(), *context_.theme,
      variant, &DateTimePage::handle_event, this);
  if (button != nullptr) {
    apply_fixed_font(lv_obj_get_child(button, 0), id);
  }
  return button;
}

bool DateTimePage::create_header(lv_obj_t* parent) noexcept {
  auto& theme = *context_.theme;
  PageHeaderSpec header_spec;
  header_spec.title = TextId::page_date_time_title;
  header_spec.locale = context_.locale;
  header_spec.back_event = &DateTimePage::handle_event;
  header_spec.locale_ko_event = &DateTimePage::handle_event;
  header_spec.locale_en_event = &DateTimePage::handle_event;
  header_spec.theme_event = &DateTimePage::handle_event;
  header_spec.user_data = this;
  const PageHeaderWidgets header =
      create_page_header(parent, header_spec, theme);
  if (!header) {
    return false;
  }
  back_button_ = header.back_button;
  locale_ko_button_ = header.locale_ko_button;
  locale_en_button_ = header.locale_en_button;
  theme_button_ = header.theme_button;
  return true;
}

bool DateTimePage::create_body(lv_obj_t* parent) noexcept {
  auto& theme = *context_.theme;
  lv_obj_t* body = lv_obj_create(parent);
  if (body == nullptr) {
    return false;
  }
  configure_container(body, theme, false);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_style_pad_column(body, 12, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

  lv_obj_t* summary = lv_obj_create(body);
  lv_obj_t* editor = lv_obj_create(body);
  if (summary == nullptr || editor == nullptr) {
    return false;
  }
  configure_container(summary, theme, true);
  configure_container(editor, theme, true);
  lv_obj_set_size(summary, 350, LV_PCT(100));
  lv_obj_set_size(editor, 0, LV_PCT(100));
  lv_obj_set_flex_grow(editor, 1);
  lv_obj_set_style_pad_all(summary, 10, 0);
  lv_obj_set_style_pad_row(summary, 8, 0);
  lv_obj_set_flex_flow(summary, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(editor, 18, 0);
  lv_obj_set_style_pad_row(editor, 10, 0);
  lv_obj_set_flex_flow(editor, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* current_group = create_summary_group(summary, theme, 3U);
  lv_obj_t* source_group = create_summary_group(summary, theme, 2U);
  lv_obj_t* rtc_group = create_summary_group(summary, theme, 2U);
  lv_obj_t* network_group = create_summary_group(summary, theme, 2U);
  if (current_group == nullptr || source_group == nullptr ||
      rtc_group == nullptr || network_group == nullptr) {
    return false;
  }

  lv_obj_t* current_title = create_label(
      current_group, TextId::date_time_current_title,
      hmi::ui::TextRole::kTitle);
  current_date_label_ = create_label(
      current_group, TextId::date_time_value_unset,
      hmi::ui::TextRole::kDisplay);
  current_time_label_ = create_label(
      current_group, TextId::date_time_value_unset,
      hmi::ui::TextRole::kDisplay);
  lv_obj_t* source_title = create_label(
      source_group, TextId::date_time_source_title,
      hmi::ui::TextRole::kMuted);
  source_value_label_ = create_label(
      source_group, TextId::date_time_source_unset,
      hmi::ui::TextRole::kAccent);
  lv_obj_t* rtc_title = create_label(
      rtc_group, TextId::date_time_rtc_title, hmi::ui::TextRole::kMuted);
  rtc_value_label_ = create_label(
      rtc_group, TextId::date_time_rtc_unsupported,
      hmi::ui::TextRole::kBody);
  lv_obj_t* network_title = create_label(
      network_group, TextId::date_time_network_title,
      hmi::ui::TextRole::kMuted);
  network_value_label_ = create_label(
      network_group, TextId::date_time_network_idle,
      hmi::ui::TextRole::kBody);
  if (current_title == nullptr || current_date_label_ == nullptr ||
      current_time_label_ == nullptr || source_title == nullptr ||
      source_value_label_ == nullptr || rtc_title == nullptr ||
      rtc_value_label_ == nullptr || network_title == nullptr ||
      network_value_label_ == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(rtc_value_label_, 310, 44, 5,
                               LV_TEXT_ALIGN_LEFT);
  hmi::ui::configure_multiline(network_value_label_, 310, 44, 5,
                               LV_TEXT_ALIGN_LEFT);

  lv_obj_t* editor_title = create_label(
      editor, TextId::date_time_editor_title, hmi::ui::TextRole::kTitle);
  lv_obj_t* editor_note = create_label(
      editor, TextId::date_time_editor_note, hmi::ui::TextRole::kMuted);
  if (editor_title == nullptr || editor_note == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(editor_note, 570, 42, 5);

  lv_obj_t* date_row = lv_obj_create(editor);
  lv_obj_t* time_row = lv_obj_create(editor);
  lv_obj_t* action_row = lv_obj_create(editor);
  if (date_row == nullptr || time_row == nullptr || action_row == nullptr) {
    return false;
  }
  for (lv_obj_t* row : {date_row, time_row, action_row}) {
    configure_container(row, theme, false);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
  }
  lv_obj_set_height(date_row, 116);
  lv_obj_set_height(time_row, 116);
  lv_obj_set_height(action_row, 50);
  if (!create_field(date_row, 0U, TextId::date_time_field_year) ||
      !create_field(date_row, 1U, TextId::date_time_field_month) ||
      !create_field(date_row, 2U, TextId::date_time_field_day) ||
      !create_field(time_row, 3U, TextId::date_time_field_hour) ||
      !create_field(time_row, 4U, TextId::date_time_field_minute) ||
      !create_field(time_row, 5U, TextId::date_time_field_second)) {
    return false;
  }
  lv_obj_t* action_spacer = lv_obj_create(action_row);
  reset_button_ = create_button(action_row, TextId::date_time_action_reset,
                                hmi::ui::ButtonVariant::kSecondary);
  apply_button_ = create_button(action_row, TextId::date_time_action_apply,
                                hmi::ui::ButtonVariant::kPrimary);
  if (action_spacer == nullptr || reset_button_ == nullptr ||
      apply_button_ == nullptr) {
    return false;
  }
  configure_container(action_spacer, theme, false);
  lv_obj_set_height(action_spacer, 1);
  lv_obj_set_flex_grow(action_spacer, 1);
  lv_obj_set_size(reset_button_, 132, 46);
  lv_obj_set_size(apply_button_, 150, 46);
  return true;
}

bool DateTimePage::create_field(lv_obj_t* parent, const std::size_t index,
                                const TextId title) noexcept {
  if (index >= kFields.size()) {
    return false;
  }
  auto& theme = *context_.theme;
  lv_obj_t* field = lv_obj_create(parent);
  if (field == nullptr) {
    return false;
  }
  configure_container(field, theme, false);
  lv_obj_set_height(field, LV_PCT(100));
  lv_obj_set_width(field, 0);
  lv_obj_set_flex_grow(field, 1);
  lv_obj_set_style_pad_all(field, 0, 0);
  lv_obj_set_style_pad_row(field, 5, 0);
  lv_obj_set_flex_flow(field, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(field, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* title_label = create_label(field, title, hmi::ui::TextRole::kMuted);
  draft_value_labels_[index] = create_label(
      field, TextId::date_time_value_unset, hmi::ui::TextRole::kTitle);
  lv_obj_t* controls = lv_obj_create(field);
  if (title_label == nullptr || draft_value_labels_[index] == nullptr ||
      controls == nullptr) {
    return false;
  }
  configure_container(controls, theme, false);
  lv_obj_set_size(controls, LV_PCT(100), 42);
  lv_obj_set_style_pad_all(controls, 0, 0);
  lv_obj_set_style_pad_column(controls, 8, 0);
  lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  decrement_buttons_[index] = create_button(
      controls, TextId::date_time_action_decrease,
      hmi::ui::ButtonVariant::kSecondary);
  increment_buttons_[index] = create_button(
      controls, TextId::date_time_action_increase,
      hmi::ui::ButtonVariant::kSecondary);
  if (decrement_buttons_[index] == nullptr ||
      increment_buttons_[index] == nullptr) {
    return false;
  }
  lv_obj_set_size(decrement_buttons_[index], 50, 40);
  lv_obj_set_size(increment_buttons_[index], 50, 40);
  return true;
}

bool DateTimePage::create_footer(lv_obj_t* parent) noexcept {
  lv_obj_t* footer = lv_obj_create(parent);
  if (footer == nullptr) {
    return false;
  }
  context_.theme->apply_status(footer);
  lv_obj_set_size(footer, LV_PCT(100), 48);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
  result_label_ = create_label(
      footer, TextId::date_time_apply_ready, hmi::ui::TextRole::kAccent);
  if (result_label_ == nullptr) {
    return false;
  }
  // 다른 page의 status와 같은 좌측 위치를 유지하고 세로축만 footer 중앙에 맞춘다.
  hmi::ui::configure_multiline(result_label_, 940, 38, 5,
                               LV_TEXT_ALIGN_LEFT);
  lv_obj_set_height(result_label_, LV_SIZE_CONTENT);
  lv_obj_align(result_label_, LV_ALIGN_LEFT_MID, 16, 0);
  return true;
}

void DateTimePage::handle_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<DateTimePage*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  lv_obj_t* target = lv_event_get_target(event);
  if (target == self->back_button_) {
    self->context_.request_dashboard(self->context_.user_data);
  } else if (target == self->theme_button_) {
    self->context_.request_theme_toggle(self->context_.user_data);
  } else if (target == self->locale_ko_button_) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::ko_kr);
  } else if (target == self->locale_en_button_) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::en_us);
  } else if (target == self->reset_button_) {
    self->reset_draft();
    self->refresh_draft();
    self->refresh_status();
  } else if (target == self->apply_button_) {
    static_cast<void>(self->apply_draft());
  } else {
    for (std::size_t index = 0U; index < kFields.size(); ++index) {
      if (target == self->decrement_buttons_[index]) {
        static_cast<void>(self->adjust(kFields[index], -1));
        return;
      }
      if (target == self->increment_buttons_[index]) {
        static_cast<void>(self->adjust(kFields[index], 1));
        return;
      }
    }
  }
}

void DateTimePage::handle_timer(lv_timer_t* timer) noexcept {
  auto* self = timer == nullptr
                   ? nullptr
                   : static_cast<DateTimePage*>(timer->user_data);
  if (self != nullptr) {
    const std::uint32_t now_ms = lv_tick_get();
    const std::uint32_t elapsed_ms = now_ms - self->editor_tick_ms_;
    self->editor_tick_ms_ = now_ms;
    const std::uint64_t accumulated =
        static_cast<std::uint64_t>(self->editor_remainder_ms_) + elapsed_ms;
    self->editor_remainder_ms_ =
        static_cast<std::uint32_t>(accumulated % 1000U);
    self->advance_draft_seconds(
        static_cast<std::uint32_t>(accumulated / 1000U));
    self->refresh_current();
    self->refresh_status();
  }
}

bool DateTimePage::adjust(const DateTimeField field, const int delta) noexcept {
  if (delta == 0) {
    return true;
  }
  auto wrap = [delta](const int value, const int minimum,
                      const int maximum) noexcept {
    const int count = maximum - minimum + 1;
    int result = value - minimum + delta;
    result %= count;
    if (result < 0) {
      result += count;
    }
    return minimum + result;
  };
  switch (field) {
    case DateTimeField::year: {
      const int candidate = static_cast<int>(state_.draft.year) + delta;
      state_.draft.year = static_cast<std::uint16_t>(
          candidate < 2000 ? 2000 : (candidate > 2099 ? 2099 : candidate));
      clamp_day();
      break;
    }
    case DateTimeField::month:
      state_.draft.month = static_cast<std::uint8_t>(
          wrap(state_.draft.month, 1, 12));
      clamp_day();
      break;
    case DateTimeField::day: {
      const int maximum = hmi::time::days_in_month(
          state_.draft.year, state_.draft.month);
      state_.draft.day = static_cast<std::uint8_t>(
          wrap(state_.draft.day, 1, maximum));
      break;
    }
    case DateTimeField::hour:
      state_.draft.hour = static_cast<std::uint8_t>(
          wrap(state_.draft.hour, 0, 23));
      break;
    case DateTimeField::minute:
      state_.draft.minute = static_cast<std::uint8_t>(
          wrap(state_.draft.minute, 0, 59));
      break;
    case DateTimeField::second:
      state_.draft.second = static_cast<std::uint8_t>(
          wrap(state_.draft.second, 0, 59));
      break;
  }
  state_.draft_modified = true;
  state_.has_apply_result = false;
  refresh_draft();
  refresh_status();
  return true;
}

void DateTimePage::advance_draft_seconds(const std::uint32_t seconds) noexcept {
  if (seconds == 0U || !hmi::time::is_valid(state_.draft)) {
    return;
  }
  hmi::time::advance_seconds(state_.draft, seconds);
  refresh_draft();
}

bool DateTimePage::set_draft(
    const hmi::time::DateTimeValue& value) noexcept {
  if (!hmi::time::is_valid(value)) {
    state_.last_apply = hmi::time::ManualApplyResult::invalid_value;
    state_.has_apply_result = true;
    refresh_status();
    return false;
  }
  state_.draft = value;
  state_.draft_modified = true;
  state_.has_apply_result = false;
  refresh_draft();
  refresh_status();
  return true;
}

hmi::time::ManualApplyResult DateTimePage::apply_draft() noexcept {
  if (context_.time_runtime == nullptr || !context_.time_runtime->valid()) {
    state_.last_apply = hmi::time::ManualApplyResult::system_write_failed;
  } else {
    state_.last_apply =
        context_.time_runtime->controller->apply_manual(state_.draft);
  }
  state_.has_apply_result = true;
  if (state_.last_apply == hmi::time::ManualApplyResult::applied ||
      state_.last_apply ==
          hmi::time::ManualApplyResult::applied_rtc_write_failed) {
    state_.draft_modified = false;
    refresh_current();
  }
  refresh();
  return state_.last_apply;
}

void DateTimePage::reset_draft() noexcept {
  if (state_.has_current) {
    state_.draft = state_.current;
  } else {
    state_.draft = {2026U, 1U, 1U, 0U, 0U, 0U};
  }
  state_.draft_modified = false;
  state_.has_apply_result = false;
}

void DateTimePage::clamp_day() noexcept {
  const std::uint8_t maximum =
      hmi::time::days_in_month(state_.draft.year, state_.draft.month);
  if (state_.draft.day > maximum) {
    state_.draft.day = maximum;
  }
}

void DateTimePage::refresh() noexcept {
  refresh_current();
  refresh_draft();
  refresh_status();
}

void DateTimePage::refresh_current() noexcept {
  if (context_.time_runtime == nullptr || !context_.time_runtime->valid()) {
    state_.has_current = false;
    state_.source = hmi::time::TimeSource::unset;
    return;
  }
  const auto& model = *context_.time_runtime->model;
  state_.has_current = model.has_value();
  state_.source = model.source();
  if (state_.has_current) {
    state_.current = model.value();
  }
  if (current_date_label_ == nullptr || current_time_label_ == nullptr) {
    return;
  }
  if (!state_.has_current) {
    const char* unset = generated::text(
        context_.locale, TextId::date_time_value_unset).data();
    lv_label_set_text(current_date_label_, unset);
    lv_label_set_text(current_time_label_, unset);
    return;
  }
  std::array<char, 24U> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%04u-%02u-%02u",
                static_cast<unsigned>(state_.current.year),
                static_cast<unsigned>(state_.current.month),
                static_cast<unsigned>(state_.current.day));
  lv_label_set_text(current_date_label_, buffer.data());
  std::snprintf(buffer.data(), buffer.size(), "%02u:%02u:%02u",
                static_cast<unsigned>(state_.current.hour),
                static_cast<unsigned>(state_.current.minute),
                static_cast<unsigned>(state_.current.second));
  lv_label_set_text(current_time_label_, buffer.data());
}

void DateTimePage::refresh_draft() noexcept {
  if (draft_value_labels_[0] == nullptr) {
    return;
  }
  std::array<char, 8U> buffer{};
  const std::array<unsigned, 6U> values = {
      state_.draft.year, state_.draft.month, state_.draft.day,
      state_.draft.hour, state_.draft.minute, state_.draft.second};
  for (std::size_t index = 0U; index < values.size(); ++index) {
    std::snprintf(buffer.data(), buffer.size(), index == 0U ? "%04u" : "%02u",
                  values[index]);
    lv_label_set_text(draft_value_labels_[index], buffer.data());
  }
}

void DateTimePage::refresh_status() noexcept {
  if (context_.time_runtime != nullptr && context_.time_runtime->valid()) {
    state_.sync = context_.time_runtime->controller->snapshot();
  }
  const TextId source_id = source_text(state_.source);
  const TextId rtc_id = rtc_text(state_.sync.rtc_status);
  const TextId network_id = network_text(state_.sync.network_state);
  const TextId result_id = apply_text(state_);
  if (source_value_label_ != nullptr) {
    lv_label_set_text(source_value_label_,
                      generated::text(context_.locale, source_id).data());
    apply_fixed_font(source_value_label_, source_id);
  }
  if (rtc_value_label_ != nullptr) {
    lv_label_set_text(rtc_value_label_,
                      generated::text(context_.locale, rtc_id).data());
    apply_fixed_font(rtc_value_label_, rtc_id);
  }
  if (network_value_label_ != nullptr) {
    lv_label_set_text(network_value_label_,
                      generated::text(context_.locale, network_id).data());
    apply_fixed_font(network_value_label_, network_id);
  }
  if (result_label_ != nullptr) {
    lv_label_set_text(result_label_,
                      generated::text(context_.locale, result_id).data());
    apply_fixed_font(result_label_, result_id);
    lv_obj_align(result_label_, LV_ALIGN_LEFT_MID, 16, 0);
  }
}

DateTimePageSnapshot DateTimePage::date_time_snapshot() const noexcept {
  return state_;
}

void DateTimePage::destroy() noexcept {
  if (timer_ != nullptr) {
    lv_timer_del(timer_);
    timer_ = nullptr;
  }
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  back_button_ = nullptr;
  theme_button_ = nullptr;
  locale_ko_button_ = nullptr;
  locale_en_button_ = nullptr;
  current_date_label_ = nullptr;
  current_time_label_ = nullptr;
  source_value_label_ = nullptr;
  rtc_value_label_ = nullptr;
  network_value_label_ = nullptr;
  draft_value_labels_.fill(nullptr);
  decrement_buttons_.fill(nullptr);
  increment_buttons_.fill(nullptr);
  reset_button_ = nullptr;
  apply_button_ = nullptr;
  result_label_ = nullptr;
  editor_tick_ms_ = 0U;
  editor_remainder_ms_ = 0U;
  context_ = {};
  state_ = {};
}

}  // namespace hmi::samples::suite
