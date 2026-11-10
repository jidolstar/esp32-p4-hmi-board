#include "pages/wav_sounds/wav_sounds_page.hpp"

#include <array>
#include <cstdio>
#include <string_view>

#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "hmi/i18n/format.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {

using TextId = generated::TextId;

void apply_fixed_font(lv_obj_t* label, TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

void apply_fixed_font(lv_obj_t* label, const char* key) noexcept {
  if (label != nullptr && key != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(key)), 0);
  }
}

lv_obj_t* create_fixed_label(lv_obj_t* parent, hmi::i18n::Locale locale,
                             TextId id, hmi::ui::UiTheme& theme,
                             hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(locale, id).data(), theme, role);
  apply_fixed_font(label, id);
  return label;
}

lv_obj_t* create_fixed_button(lv_obj_t* parent, hmi::i18n::Locale locale,
                              TextId id, hmi::ui::UiTheme& theme,
                              hmi::ui::ButtonVariant variant,
                              lv_event_cb_t callback, void* user_data) noexcept {
  lv_obj_t* button = hmi::ui::create_button(
      parent, generated::text(locale, id).data(), theme, variant, callback,
      user_data);
  if (button != nullptr) {
    apply_fixed_font(lv_obj_get_child(button, 0), id);
  }
  return button;
}

lv_obj_t* create_transparent(lv_obj_t* parent, hmi::ui::UiTheme& theme,
                             lv_flex_flow_t flow) noexcept {
  lv_obj_t* object = lv_obj_create(parent);
  if (object != nullptr) {
    theme.apply_transparent(object);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_flex_flow(object, flow);
  }
  return object;
}

TextId status_id(const WavSoundsPageSnapshot& value) noexcept {
  using hmi::audio::PlaybackError;
  using hmi::audio::PlaybackState;
  if (value.state == PlaybackState::error) {
    switch (value.error) {
      case PlaybackError::path:
      case PlaybackError::file:
        return TextId::wav_sounds_status_file;
      case PlaybackError::unsupported:
        return TextId::wav_sounds_status_unsupported;
      case PlaybackError::truncated:
        return TextId::wav_sounds_status_truncated;
      case PlaybackError::busy:
        return TextId::wav_sounds_status_busy;
      case PlaybackError::output:
        return TextId::wav_sounds_status_output;
      case PlaybackError::read:
        return TextId::wav_sounds_status_read;
      case PlaybackError::unavailable:
        return TextId::wav_sounds_status_unavailable;
      case PlaybackError::none:
        return TextId::wav_sounds_status_output;
    }
  }
  switch (value.state) {
    case PlaybackState::unavailable:
      return TextId::wav_sounds_status_unavailable;
    case PlaybackState::starting:
      return TextId::wav_sounds_status_starting;
    case PlaybackState::playing:
      return TextId::wav_sounds_status_playing;
    case PlaybackState::pausing:
      return TextId::wav_sounds_status_pausing;
    case PlaybackState::paused:
      return TextId::wav_sounds_status_paused;
    case PlaybackState::resuming:
      return TextId::wav_sounds_status_resuming;
    case PlaybackState::completed:
      return TextId::wav_sounds_status_completed;
    case PlaybackState::stopped:
      return TextId::wav_sounds_status_stopped;
    case PlaybackState::idle:
      return TextId::wav_sounds_status_ready;
    case PlaybackState::error:
      break;
  }
  return TextId::wav_sounds_status_unavailable;
}

TextId row_status_id(const WavSoundsPageSnapshot& value) noexcept {
  using hmi::audio::PlaybackState;
  if (value.state == PlaybackState::completed) {
    return TextId::wav_sounds_status_completed;
  }
  if (value.state == PlaybackState::error) {
    return TextId::wav_sounds_row_error;
  }
  if (value.state == PlaybackState::pausing) {
    return TextId::wav_sounds_status_pausing;
  }
  if (value.state == PlaybackState::paused) {
    return TextId::wav_sounds_status_paused;
  }
  if (value.state == PlaybackState::resuming) {
    return TextId::wav_sounds_status_resuming;
  }
  return TextId::wav_sounds_status_playing;
}

void set_time(lv_obj_t* label, std::uint32_t milliseconds,
              bool known) noexcept {
  if (label == nullptr) {
    return;
  }
  if (!known) {
    lv_label_set_text(label, "--:--");
    return;
  }
  const unsigned long seconds = milliseconds / 1000U;
  std::array<char, 20U> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%02lu:%02lu", seconds / 60U,
                seconds % 60U);
  lv_label_set_text(label, buffer.data());
}

}  // namespace

bool WavSoundsPage::create(lv_obj_t* parent, const PageContext& context) {
  if (root_ != nullptr || parent == nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.audio_playback == nullptr ||
      !playback_.begin(*context.audio_playback)) {
    return false;
  }
  context_ = context;
  hmi::ui::UiTheme& theme = *context_.theme;

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
  back_button_ = create_fixed_button(
      header, context_.locale, TextId::common_back, theme,
      hmi::ui::ButtonVariant::kGhost, &WavSoundsPage::handle_event, this);
  lv_obj_t* title = create_fixed_label(header, context_.locale,
                                       TextId::page_wav_sounds_title, theme,
                                       hmi::ui::TextRole::kDisplay);
  lv_obj_t* spacer = lv_obj_create(header);
  lv_obj_t* locale_ko = create_fixed_button(
      header, context_.locale, TextId::locale_ko, theme,
      context_.locale == hmi::i18n::Locale::ko_kr
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &WavSoundsPage::handle_locale_ko_event, this);
  lv_obj_t* locale_en = create_fixed_button(
      header, context_.locale, TextId::locale_en, theme,
      context_.locale == hmi::i18n::Locale::en_us
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &WavSoundsPage::handle_locale_en_event, this);
  const TextId theme_id = theme.mode() == hmi::ui::ThemeMode::kDark
                              ? TextId::theme_light
                              : TextId::theme_dark;
  lv_obj_t* theme_button = create_fixed_button(
      header, context_.locale, theme_id, theme,
      hmi::ui::ButtonVariant::kGhost, &WavSoundsPage::handle_theme_event, this);
  if (back_button_ == nullptr || title == nullptr || spacer == nullptr ||
      locale_ko == nullptr || locale_en == nullptr || theme_button == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(back_button_, 86, 44);
  theme.apply_transparent(spacer);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_size(locale_ko, 88, 42);
  lv_obj_set_size(locale_en, 88, 42);
  lv_obj_set_size(theme_button, 104, 42);

  lv_obj_t* body = create_transparent(root_, theme, LV_FLEX_FLOW_ROW);
  if (body == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_style_pad_column(body, 12, 0);

  lv_obj_t* list_card = lv_obj_create(body);
  lv_obj_t* right_column = create_transparent(body, theme, LV_FLEX_FLOW_COLUMN);
  if (list_card == nullptr || right_column == nullptr) {
    destroy();
    return false;
  }
  theme.apply_card(list_card);
  lv_obj_set_size(list_card, 430, LV_PCT(100));
  lv_obj_clear_flag(list_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(list_card, 16, 0);
  lv_obj_set_style_pad_row(list_card, 6, 0);
  lv_obj_set_flex_flow(list_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_width(right_column, 0);
  lv_obj_set_height(right_column, LV_PCT(100));
  lv_obj_set_flex_grow(right_column, 1);
  lv_obj_set_style_pad_row(right_column, 12, 0);

  lv_obj_t* list_header = create_transparent(list_card, theme, LV_FLEX_FLOW_ROW);
  if (list_header == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(list_header, LV_PCT(100), 32);
  lv_obj_set_flex_align(list_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_t* list_title = create_fixed_label(
      list_header, context_.locale, TextId::wav_sounds_list_title, theme,
      hmi::ui::TextRole::kTitle);
  lv_obj_t* list_header_spacer = lv_obj_create(list_header);
  lv_obj_t* list_count = create_fixed_label(
      list_header, context_.locale, TextId::wav_sounds_list_count, theme,
      hmi::ui::TextRole::kAccent);
  if (list_title == nullptr || list_header_spacer == nullptr ||
      list_count == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(list_header_spacer);
  lv_obj_set_height(list_header_spacer, 1);
  lv_obj_set_flex_grow(list_header_spacer, 1);
  std::array<char, 40U> count_buffer{};
  const hmi::i18n::FormatArgument count_argument =
      hmi::i18n::FormatArgument::number("count", generated::kWavCatalog.size());
  if (hmi::i18n::format_to(generated::text(context_.locale,
                                            TextId::wav_sounds_list_count),
                            &count_argument, 1U, count_buffer.data(),
                            count_buffer.size())
          .status == hmi::i18n::FormatStatus::ok) {
    lv_label_set_text(list_count, count_buffer.data());
  }

  for (std::size_t index = 0U; index < generated::kWavCatalog.size(); ++index) {
    const auto& entry = generated::kWavCatalog[index];
    lv_obj_t* row = lv_obj_create(list_card);
    if (row == nullptr) {
      destroy();
      return false;
    }
    rows_[index] = row;
    theme.apply_list_item(row);
    lv_obj_set_size(row, LV_PCT(100), 65);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_set_style_pad_column(row, 10, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(row, &WavSoundsPage::handle_event,
                        LV_EVENT_SHORT_CLICKED, this);
    lv_obj_t* text_column = create_transparent(row, theme, LV_FLEX_FLOW_COLUMN);
    lv_obj_t* state_label = hmi::ui::create_label(
        row, "", theme, hmi::ui::TextRole::kAccent);
    if (text_column == nullptr || state_label == nullptr) {
      destroy();
      return false;
    }
    // 투명한 자식 container가 pointer hit target을 가로채면 row callback이 호출되지 않는다.
    // 행 내부 표시는 모두 장식이므로 clickable을 제거해 65 px 전체를 하나의 touch target으로 둔다.
    lv_obj_clear_flag(text_column, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(state_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(text_column, 0);
    lv_obj_set_height(text_column, LV_PCT(100));
    lv_obj_set_flex_grow(text_column, 1);
    lv_obj_set_style_pad_row(text_column, 2, 0);
    lv_obj_t* row_title = hmi::ui::create_label(
        text_column, generated::text(context_.locale, entry.title_key).data(),
        theme, hmi::ui::TextRole::kTitle);
    lv_obj_t* row_description = hmi::ui::create_label(
        text_column,
        generated::text(context_.locale, entry.description_key).data(), theme,
        hmi::ui::TextRole::kMuted);
    if (row_title == nullptr || row_description == nullptr) {
      destroy();
      return false;
    }
    lv_obj_clear_flag(row_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row_description, LV_OBJ_FLAG_CLICKABLE);
    apply_fixed_font(row_title, entry.title_key);
    apply_fixed_font(row_description, entry.description_key);
    apply_fixed_font(state_label, TextId::wav_sounds_status_playing);
    lv_obj_set_width(state_label, 92);
    lv_obj_set_style_text_align(state_label, LV_TEXT_ALIGN_RIGHT, 0);
    row_status_[index] = state_label;
  }

  lv_obj_t* list_hint = create_fixed_label(
      list_card, context_.locale, TextId::wav_sounds_list_hint, theme,
      hmi::ui::TextRole::kMuted);
  if (list_hint == nullptr) {
    destroy();
    return false;
  }

  lv_obj_t* player = lv_obj_create(right_column);
  lv_obj_t* format_card = lv_obj_create(right_column);
  if (player == nullptr || format_card == nullptr) {
    destroy();
    return false;
  }
  theme.apply_card(player);
  lv_obj_set_size(player, LV_PCT(100), 328);
  lv_obj_clear_flag(player, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(player, 18, 0);
  lv_obj_set_style_pad_row(player, 7, 0);
  lv_obj_set_flex_flow(player, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* eyebrow = create_fixed_label(
      player, context_.locale, TextId::wav_sounds_player_now_playing, theme,
      hmi::ui::TextRole::kAccent);
  selected_title_ = create_fixed_label(
      player, context_.locale, TextId::wav_sounds_player_empty_title, theme,
      hmi::ui::TextRole::kDisplay);
  selected_description_ = create_fixed_label(
      player, context_.locale, TextId::wav_sounds_player_empty_body, theme,
      hmi::ui::TextRole::kMuted);
  selected_format_ = create_fixed_label(
      player, context_.locale, TextId::wav_sounds_player_format, theme,
      hmi::ui::TextRole::kMuted);
  progress_bar_ = lv_bar_create(player);
  if (eyebrow == nullptr || selected_title_ == nullptr ||
      selected_description_ == nullptr || selected_format_ == nullptr ||
      progress_bar_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_width(selected_description_, LV_PCT(100));
  lv_obj_set_height(selected_description_, 24);
  lv_label_set_long_mode(selected_description_, LV_LABEL_LONG_DOT);
  lv_obj_set_size(progress_bar_, LV_PCT(100), 12);
  lv_bar_set_range(progress_bar_, 0, 1000);
  lv_obj_set_style_bg_color(progress_bar_, theme.accent_color(),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(progress_bar_, 999, LV_PART_MAIN);
  lv_obj_set_style_radius(progress_bar_, 999, LV_PART_INDICATOR);

  lv_obj_t* times = create_transparent(player, theme, LV_FLEX_FLOW_ROW);
  if (times == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(times, LV_PCT(100), 49);
  lv_obj_set_style_pad_column(times, 8, 0);
  const std::array<TextId, 3U> time_ids = {
      TextId::wav_sounds_time_elapsed, TextId::wav_sounds_time_total,
      TextId::wav_sounds_time_remaining};
  std::array<lv_obj_t**, 3U> time_values = {
      &elapsed_value_, &total_value_, &remaining_value_};
  for (std::size_t index = 0U; index < time_ids.size(); ++index) {
    lv_obj_t* time_column =
        create_transparent(times, theme, LV_FLEX_FLOW_COLUMN);
    if (time_column == nullptr) {
      destroy();
      return false;
    }
    lv_obj_set_width(time_column, 0);
    lv_obj_set_height(time_column, LV_PCT(100));
    lv_obj_set_flex_grow(time_column, 1);
    lv_obj_set_flex_align(time_column, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* caption = create_fixed_label(time_column, context_.locale,
                                            time_ids[index], theme,
                                            hmi::ui::TextRole::kMuted);
    *time_values[index] = hmi::ui::create_label(
        time_column, "--:--", theme, hmi::ui::TextRole::kTitle);
    if (caption == nullptr || *time_values[index] == nullptr) {
      destroy();
      return false;
    }
    apply_fixed_font(*time_values[index], TextId::wav_sounds_time_total);
  }

  status_label_ = create_fixed_label(
      player, context_.locale, TextId::wav_sounds_status_ready, theme,
      hmi::ui::TextRole::kAccent);
  lv_obj_t* actions = create_transparent(player, theme, LV_FLEX_FLOW_ROW);
  if (status_label_ == nullptr || actions == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(actions, LV_PCT(100), 50);
  lv_obj_set_style_pad_column(actions, 10, 0);
  play_button_ = create_fixed_button(
      actions, context_.locale, TextId::wav_sounds_action_play, theme,
      hmi::ui::ButtonVariant::kPrimary, &WavSoundsPage::handle_event, this);
  stop_button_ = create_fixed_button(
      actions, context_.locale, TextId::wav_sounds_action_stop, theme,
      hmi::ui::ButtonVariant::kSecondary, &WavSoundsPage::handle_event, this);
  if (play_button_ == nullptr || stop_button_ == nullptr) {
    destroy();
    return false;
  }
  play_button_label_ = lv_obj_get_child(play_button_, 0);
  lv_obj_set_size(play_button_, 0, 50);
  lv_obj_set_flex_grow(play_button_, 1);
  lv_obj_set_size(stop_button_, 150, 50);

  theme.apply_status(format_card);
  lv_obj_set_width(format_card, LV_PCT(100));
  lv_obj_set_flex_grow(format_card, 1);
  lv_obj_clear_flag(format_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(format_card, 16, 0);
  lv_obj_set_style_pad_row(format_card, 7, 0);
  lv_obj_set_flex_flow(format_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_t* format_title = create_fixed_label(
      format_card, context_.locale, TextId::wav_sounds_format_title, theme,
      hmi::ui::TextRole::kTitle);
  lv_obj_t* format_body = create_fixed_label(
      format_card, context_.locale, TextId::wav_sounds_format_body, theme,
      hmi::ui::TextRole::kMuted);
  if (format_title == nullptr || format_body == nullptr) {
    destroy();
    return false;
  }
  hmi::ui::configure_multiline(format_body, 510, 64, 5);

  refresh();
  timer_ = lv_timer_create(&WavSoundsPage::handle_timer, 100U, this);
  if (timer_ == nullptr) {
    destroy();
    return false;
  }
  return true;
}

bool WavSoundsPage::play_index(std::size_t index) noexcept {
  if (!playback_.ready() || index >= generated::kWavCatalog.size()) {
    return false;
  }
  snapshot_.selected_index = static_cast<std::uint8_t>(index);
  snapshot_.total_ms = generated::kWavCatalog[index].duration_ms;
  const bool accepted = playback_.play(generated::kWavCatalog[index].asset_path);
  refresh();
  return accepted;
}

bool WavSoundsPage::stop() noexcept {
  if (!playback_.ready()) {
    return false;
  }
  const bool accepted = playback_.stop();
  refresh();
  return accepted;
}

bool WavSoundsPage::pause() noexcept {
  if (!playback_.ready()) {
    return false;
  }
  const bool accepted = playback_.pause();
  refresh();
  return accepted;
}

bool WavSoundsPage::resume() noexcept {
  if (!playback_.ready()) {
    return false;
  }
  const bool accepted = playback_.resume();
  refresh();
  return accepted;
}

void WavSoundsPage::refresh() noexcept {
  const hmi::audio::PlaybackSnapshot source = playback_.snapshot();
  hmi::audio::PlaybackSnapshot adjusted = source;
  if (adjusted.duration_ms == 0U &&
      snapshot_.selected_index < generated::kWavCatalog.size()) {
    adjusted.duration_ms =
        generated::kWavCatalog[snapshot_.selected_index].duration_ms;
  }
  const hmi::audio::PlaybackProgress progress =
      hmi::audio::AudioPlayback::progress_of(adjusted);
  snapshot_.state = source.state;
  snapshot_.error = source.error;
  snapshot_.elapsed_ms = progress.elapsed_ms;
  snapshot_.total_ms = progress.total_ms;
  snapshot_.remaining_ms = progress.remaining_ms;
  snapshot_.revision = source.revision;
  snapshot_.progress_permille = progress.permille;
  const bool transitioning =
      source.state == hmi::audio::PlaybackState::starting ||
      source.state == hmi::audio::PlaybackState::pausing ||
      source.state == hmi::audio::PlaybackState::resuming;
  snapshot_.primary_enabled =
      snapshot_.selected_index < generated::kWavCatalog.size() &&
      source.state != hmi::audio::PlaybackState::unavailable && !transitioning;
  snapshot_.stop_enabled = source.state == hmi::audio::PlaybackState::starting ||
                           source.state == hmi::audio::PlaybackState::playing ||
                           source.state == hmi::audio::PlaybackState::pausing ||
                           source.state == hmi::audio::PlaybackState::paused ||
                           source.state == hmi::audio::PlaybackState::resuming;
  update_view();
}

void WavSoundsPage::update_view() noexcept {
  if (root_ == nullptr) {
    return;
  }
  const bool has_selection =
      snapshot_.selected_index < generated::kWavCatalog.size();
  const TextId current_status = status_id(snapshot_);
  const TextId compact_status = row_status_id(snapshot_);
  const bool show_row_status =
      snapshot_.state == hmi::audio::PlaybackState::starting ||
      snapshot_.state == hmi::audio::PlaybackState::playing ||
      snapshot_.state == hmi::audio::PlaybackState::pausing ||
      snapshot_.state == hmi::audio::PlaybackState::paused ||
      snapshot_.state == hmi::audio::PlaybackState::resuming ||
      snapshot_.state == hmi::audio::PlaybackState::completed ||
      snapshot_.state == hmi::audio::PlaybackState::error;
  for (std::size_t index = 0U; index < generated::kWavCatalog.size(); ++index) {
    const bool selected = has_selection && snapshot_.selected_index == index;
    if (rows_[index] != nullptr) {
      lv_obj_set_style_border_width(rows_[index], selected ? 2 : 0, 0);
      lv_obj_set_style_border_color(rows_[index], context_.theme->accent_color(),
                                    0);
    }
    if (row_status_[index] != nullptr) {
      lv_label_set_text(row_status_[index],
                        selected && show_row_status
                            ? generated::text(context_.locale, compact_status)
                                  .data()
                            : "");
      apply_fixed_font(row_status_[index], compact_status);
    }
  }

  if (has_selection) {
    const auto& entry = generated::kWavCatalog[snapshot_.selected_index];
    lv_label_set_text(selected_title_,
                      generated::text(context_.locale, entry.title_key).data());
    lv_label_set_text(
        selected_description_,
        generated::text(context_.locale, entry.description_key).data());
    apply_fixed_font(selected_title_, entry.title_key);
    apply_fixed_font(selected_description_, entry.description_key);
    std::array<char, 80U> format{};
    const std::array<hmi::i18n::FormatArgument, 2U> arguments = {
        hmi::i18n::FormatArgument::number("rate", entry.sample_rate_hz / 1000U),
        hmi::i18n::FormatArgument::number("bits", entry.bits_per_sample)};
    if (hmi::i18n::format_to(
            generated::text(context_.locale,
                            TextId::wav_sounds_player_format),
            arguments.data(), arguments.size(), format.data(), format.size())
            .status == hmi::i18n::FormatStatus::ok) {
      lv_label_set_text(selected_format_, format.data());
    }
  } else {
    lv_label_set_text(
        selected_title_,
        generated::text(context_.locale,
                        TextId::wav_sounds_player_empty_title)
            .data());
    lv_label_set_text(
        selected_description_,
        generated::text(context_.locale,
                        TextId::wav_sounds_player_empty_body)
            .data());
    lv_label_set_text(selected_format_, "");
    apply_fixed_font(selected_title_, TextId::wav_sounds_player_empty_title);
    apply_fixed_font(selected_description_,
                     TextId::wav_sounds_player_empty_body);
  }

  if (progress_bar_ != nullptr) {
    lv_bar_set_value(progress_bar_, snapshot_.progress_permille, LV_ANIM_OFF);
  }
  const bool duration_known = snapshot_.total_ms > 0U;
  set_time(elapsed_value_, snapshot_.elapsed_ms, has_selection);
  set_time(total_value_, snapshot_.total_ms, duration_known);
  set_time(remaining_value_, snapshot_.remaining_ms, duration_known);
  if (status_label_ != nullptr) {
    lv_label_set_text(status_label_,
                      generated::text(context_.locale, current_status).data());
    apply_fixed_font(status_label_, current_status);
  }
  if (play_button_label_ != nullptr) {
    TextId action = TextId::wav_sounds_action_play;
    if (snapshot_.state == hmi::audio::PlaybackState::playing ||
        snapshot_.state == hmi::audio::PlaybackState::pausing) {
      action = TextId::wav_sounds_action_pause;
    } else if (snapshot_.state == hmi::audio::PlaybackState::paused ||
               snapshot_.state == hmi::audio::PlaybackState::resuming) {
      action = TextId::wav_sounds_action_resume;
    } else if (snapshot_.state == hmi::audio::PlaybackState::completed) {
      action = TextId::wav_sounds_action_replay;
    }
    lv_label_set_text(play_button_label_,
                      generated::text(context_.locale, action).data());
    apply_fixed_font(play_button_label_, action);
  }
  if (play_button_ != nullptr) {
    if (snapshot_.primary_enabled) {
      lv_obj_clear_state(play_button_, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(play_button_, LV_STATE_DISABLED);
    }
  }
  if (stop_button_ != nullptr) {
    if (snapshot_.stop_enabled) {
      lv_obj_clear_state(stop_button_, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(stop_button_, LV_STATE_DISABLED);
    }
  }
}

void WavSoundsPage::handle_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<WavSoundsPage*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  lv_obj_t* target = lv_event_get_target(event);
  if (target == self->back_button_) {
    self->context_.request_dashboard(self->context_.user_data);
    return;
  }
  if (target == self->play_button_ && self->snapshot_.primary_enabled) {
    if (self->snapshot_.state == hmi::audio::PlaybackState::playing) {
      self->pause();
    } else if (self->snapshot_.state == hmi::audio::PlaybackState::paused) {
      self->resume();
    } else {
      self->play_index(self->snapshot_.selected_index);
    }
    return;
  }
  if (target == self->stop_button_ && self->snapshot_.stop_enabled) {
    self->stop();
    return;
  }
  for (std::size_t index = 0U; index < generated::kWavCatalog.size(); ++index) {
    if (target == self->rows_[index]) {
      self->play_index(index);
      return;
    }
  }
}

void WavSoundsPage::handle_locale_ko_event(lv_event_t* event) {
  if (event != nullptr &&
      lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    auto* self = static_cast<WavSoundsPage*>(lv_event_get_user_data(event));
    if (self != nullptr) {
      self->context_.set_locale(self->context_.user_data,
                                hmi::i18n::Locale::ko_kr);
    }
  }
}

void WavSoundsPage::handle_locale_en_event(lv_event_t* event) {
  if (event != nullptr &&
      lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    auto* self = static_cast<WavSoundsPage*>(lv_event_get_user_data(event));
    if (self != nullptr) {
      self->context_.set_locale(self->context_.user_data,
                                hmi::i18n::Locale::en_us);
    }
  }
}

void WavSoundsPage::handle_theme_event(lv_event_t* event) {
  if (event != nullptr &&
      lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    auto* self = static_cast<WavSoundsPage*>(lv_event_get_user_data(event));
    if (self != nullptr) {
      self->context_.request_theme_toggle(self->context_.user_data);
    }
  }
}

void WavSoundsPage::handle_timer(lv_timer_t* timer) noexcept {
  if (timer != nullptr) {
    auto* self = static_cast<WavSoundsPage*>(timer->user_data);
    if (self != nullptr) {
      self->refresh();
    }
  }
}

void WavSoundsPage::reset_widgets() noexcept {
  root_ = nullptr;
  back_button_ = nullptr;
  rows_.fill(nullptr);
  row_status_.fill(nullptr);
  selected_title_ = nullptr;
  selected_description_ = nullptr;
  selected_format_ = nullptr;
  status_label_ = nullptr;
  progress_bar_ = nullptr;
  elapsed_value_ = nullptr;
  total_value_ = nullptr;
  remaining_value_ = nullptr;
  play_button_ = nullptr;
  play_button_label_ = nullptr;
  stop_button_ = nullptr;
}

void WavSoundsPage::destroy() noexcept {
  if (timer_ != nullptr) {
    lv_timer_del(timer_);
  }
  timer_ = nullptr;
  playback_.end();
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  reset_widgets();
  context_ = {};
  snapshot_ = {};
}

WavSoundsPage::~WavSoundsPage() { destroy(); }

}  // namespace hmi::samples::suite
