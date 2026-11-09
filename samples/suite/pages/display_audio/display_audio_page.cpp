#include "pages/display_audio/display_audio_page.hpp"

#include <array>

#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "hmi/i18n/format.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {

void apply_fixed_font(lv_obj_t* label, generated::TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

lv_obj_t* create_label(lv_obj_t* parent, hmi::i18n::Locale locale,
                       generated::TextId id, hmi::ui::UiTheme& theme,
                       hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(locale, id).data(), theme, role);
  apply_fixed_font(label, id);
  return label;
}

lv_obj_t* create_button(lv_obj_t* parent, hmi::i18n::Locale locale,
                        generated::TextId id, hmi::ui::UiTheme& theme,
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

lv_obj_t* create_panel(lv_obj_t* parent, hmi::ui::UiTheme& theme) noexcept {
  lv_obj_t* panel = lv_obj_create(parent);
  if (panel != nullptr) {
    theme.apply_card(panel);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_style_pad_column(panel, 22, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
  }
  return panel;
}

lv_obj_t* create_column(lv_obj_t* parent, hmi::ui::UiTheme& theme,
                        lv_coord_t width) noexcept {
  lv_obj_t* column = lv_obj_create(parent);
  if (column != nullptr) {
    theme.apply_transparent(column);
    lv_obj_set_size(column, width, LV_PCT(100));
    if (width == 0) {
      lv_obj_set_flex_grow(column, 1);
    }
    lv_obj_clear_flag(column, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(column, 0, 0);
    lv_obj_set_style_pad_row(column, 10, 0);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  }
  return column;
}

lv_obj_t* create_row(lv_obj_t* parent, hmi::ui::UiTheme& theme,
                     lv_coord_t height) noexcept {
  lv_obj_t* row = lv_obj_create(parent);
  if (row != nullptr) {
    theme.apply_transparent(row);
    lv_obj_set_size(row, LV_PCT(100), height);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 24, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
  }
  return row;
}

void configure_slider(lv_obj_t* slider, hmi::ui::UiTheme& theme) noexcept {
  if (slider == nullptr) {
    return;
  }
  const bool light = theme.mode() == hmi::ui::ThemeMode::kLight;
  const lv_color_t track =
      light ? lv_color_hex(0xCCD9E6U) : lv_color_hex(0x314B66U);
  const lv_color_t active =
      light ? lv_color_hex(0x5B5CE2U) : lv_color_hex(0x7C74F8U);
  const lv_color_t thumb =
      light ? lv_color_hex(0xFFFFFFU) : lv_color_hex(0xF7FAFFU);
  lv_obj_set_style_bg_color(slider, track, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, 999, LV_PART_MAIN);
  lv_obj_set_style_border_color(slider, track, LV_PART_MAIN);
  lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, active, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, 999, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, thumb, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_width(slider, 36, LV_PART_KNOB);
  lv_obj_set_style_height(slider, 36, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, 999, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, active, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 4, LV_PART_KNOB);
  lv_obj_set_style_shadow_color(slider, lv_color_hex(0x162033U), LV_PART_KNOB);
  lv_obj_set_style_shadow_width(slider, 12, LV_PART_KNOB);
  lv_obj_set_style_shadow_opa(slider, light ? LV_OPA_20 : LV_OPA_40,
                              LV_PART_KNOB);
}

generated::TextId status_text(SettingsPageStatus status) noexcept {
  switch (status) {
    case SettingsPageStatus::kSaved:
      return generated::TextId::display_audio_status_saved;
    case SettingsPageStatus::kApplyFailed:
      return generated::TextId::display_audio_status_apply_failed;
    case SettingsPageStatus::kSaveFailed:
      return generated::TextId::display_audio_status_save_failed;
    case SettingsPageStatus::kPreviewUnavailable:
      return generated::TextId::display_audio_status_preview_unavailable;
    case SettingsPageStatus::kUnavailable:
      return generated::TextId::display_audio_status_unavailable;
    case SettingsPageStatus::kReady:
      return generated::TextId::display_audio_status_ready;
  }
  return generated::TextId::display_audio_status_unavailable;
}

}  // namespace

static_assert(sizeof(DisplayAudioPage) <= 288U,
              "DisplayAudioPage must keep only scalar state and widget handles");

bool DisplayAudioPage::create(lv_obj_t* parent, const PageContext& context) {
  if (root_ != nullptr || parent == nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.level_settings == nullptr ||
      !settings_.begin(*context.level_settings)) {
    return false;
  }
  context_ = context;
  state_.brightness = settings_.brightness();
  state_.volume = settings_.volume();
  state_.settings_status = settings_.volume_pending()
                               ? SettingsPageStatus::kApplyFailed
                               : SettingsPageStatus::kReady;
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
  back_button_ = create_button(
      header, context_.locale, generated::TextId::common_back, theme,
      hmi::ui::ButtonVariant::kGhost, &DisplayAudioPage::handle_event, this);
  lv_obj_t* title = create_label(
      header, context_.locale, generated::TextId::page_display_audio_title,
      theme, hmi::ui::TextRole::kDisplay);
  lv_obj_t* spacer = lv_obj_create(header);
  lv_obj_t* locale_ko_button = create_button(
      header, context_.locale, generated::TextId::locale_ko, theme,
      context_.locale == hmi::i18n::Locale::ko_kr
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &DisplayAudioPage::handle_locale_ko_event, this);
  lv_obj_t* locale_en_button = create_button(
      header, context_.locale, generated::TextId::locale_en, theme,
      context_.locale == hmi::i18n::Locale::en_us
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &DisplayAudioPage::handle_locale_en_event, this);
  const generated::TextId theme_id =
      theme.mode() == hmi::ui::ThemeMode::kDark
          ? generated::TextId::theme_light
          : generated::TextId::theme_dark;
  lv_obj_t* theme_button = create_button(
      header, context_.locale, theme_id, theme,
      hmi::ui::ButtonVariant::kGhost, &DisplayAudioPage::handle_theme_event,
      this);
  if (back_button_ == nullptr || title == nullptr || spacer == nullptr ||
      locale_ko_button == nullptr || locale_en_button == nullptr ||
      theme_button == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(back_button_, 86, 44);
  theme.apply_transparent(spacer);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_size(locale_ko_button, 88, 42);
  lv_obj_set_size(locale_en_button, 88, 42);
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
  lv_obj_set_style_pad_row(body, 12, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* brightness_panel = create_panel(body, theme);
  lv_obj_t* volume_panel = create_panel(body, theme);
  if (brightness_panel == nullptr || volume_panel == nullptr) {
    destroy();
    return false;
  }

  lv_obj_t* brightness_info = create_column(brightness_panel, theme, 285);
  lv_obj_t* brightness_controls = create_column(brightness_panel, theme, 0);
  if (brightness_info == nullptr || brightness_controls == nullptr) {
    destroy();
    return false;
  }
  lv_obj_t* brightness_title = create_label(
      brightness_info, context_.locale,
      generated::TextId::display_audio_brightness_title, theme,
      hmi::ui::TextRole::kTitle);
  lv_obj_t* brightness_description = create_label(
      brightness_info, context_.locale,
      generated::TextId::display_audio_brightness_description, theme,
      hmi::ui::TextRole::kMuted);
  lv_obj_t* brightness_value_row = create_row(brightness_controls, theme, 48);
  brightness_value_ = create_label(
      brightness_value_row, context_.locale,
      generated::TextId::display_audio_brightness_value, theme,
      hmi::ui::TextRole::kDisplay);
  lv_obj_t* brightness_slider_row = create_row(brightness_controls, theme, 62);
  if (brightness_slider_row != nullptr) {
    lv_obj_set_style_pad_column(brightness_slider_row, 42, 0);
  }
  brightness_down_ = create_button(
      brightness_slider_row, context_.locale,
      generated::TextId::display_audio_action_decrease, theme,
      hmi::ui::ButtonVariant::kSecondary, &DisplayAudioPage::handle_event,
      this);
  brightness_slider_ = lv_slider_create(brightness_slider_row);
  brightness_up_ = create_button(
      brightness_slider_row, context_.locale,
      generated::TextId::display_audio_action_increase, theme,
      hmi::ui::ButtonVariant::kPrimary, &DisplayAudioPage::handle_event, this);
  if (brightness_title == nullptr || brightness_description == nullptr ||
      brightness_value_row == nullptr || brightness_value_ == nullptr ||
      brightness_slider_row == nullptr || brightness_slider_ == nullptr ||
      brightness_down_ == nullptr || brightness_up_ == nullptr) {
    destroy();
    return false;
  }
  hmi::ui::configure_multiline(brightness_description, 285, 54, 4);
  lv_obj_set_size(brightness_down_, 58, 58);
  lv_obj_set_size(brightness_slider_, 0, 14);
  lv_obj_set_flex_grow(brightness_slider_, 1);
  configure_slider(brightness_slider_, theme);
  lv_obj_set_size(brightness_up_, 58, 58);
  lv_slider_set_range(brightness_slider_, 1, 10);
  lv_obj_add_event_cb(brightness_slider_, &DisplayAudioPage::handle_event,
                      LV_EVENT_VALUE_CHANGED, this);

  lv_obj_t* volume_info = create_column(volume_panel, theme, 285);
  lv_obj_t* volume_controls = create_column(volume_panel, theme, 0);
  if (volume_info == nullptr || volume_controls == nullptr) {
    destroy();
    return false;
  }
  lv_obj_t* volume_title = create_label(
      volume_info, context_.locale,
      generated::TextId::display_audio_volume_title, theme,
      hmi::ui::TextRole::kTitle);
  lv_obj_t* volume_description = create_label(
      volume_info, context_.locale,
      generated::TextId::display_audio_volume_description, theme,
      hmi::ui::TextRole::kMuted);
  lv_obj_t* volume_value_row = create_row(volume_controls, theme, 48);
  volume_value_ = create_label(
      volume_value_row, context_.locale,
      generated::TextId::display_audio_volume_value, theme,
      hmi::ui::TextRole::kDisplay);
  preview_button_ = create_button(
      volume_value_row, context_.locale,
      generated::TextId::display_audio_action_play, theme,
      hmi::ui::ButtonVariant::kGhost, &DisplayAudioPage::handle_event, this);
  lv_obj_t* volume_slider_row = create_row(volume_controls, theme, 62);
  if (volume_slider_row != nullptr) {
    lv_obj_set_style_pad_column(volume_slider_row, 42, 0);
  }
  volume_down_ = create_button(
      volume_slider_row, context_.locale,
      generated::TextId::display_audio_action_decrease, theme,
      hmi::ui::ButtonVariant::kSecondary, &DisplayAudioPage::handle_event,
      this);
  volume_slider_ = lv_slider_create(volume_slider_row);
  volume_up_ = create_button(
      volume_slider_row, context_.locale,
      generated::TextId::display_audio_action_increase, theme,
      hmi::ui::ButtonVariant::kPrimary, &DisplayAudioPage::handle_event, this);
  if (volume_title == nullptr || volume_description == nullptr ||
      volume_value_row == nullptr || volume_value_ == nullptr ||
      preview_button_ == nullptr || volume_slider_row == nullptr ||
      volume_slider_ == nullptr || volume_down_ == nullptr ||
      volume_up_ == nullptr) {
    destroy();
    return false;
  }
  hmi::ui::configure_multiline(volume_description, 285, 54, 4);
  lv_obj_set_size(preview_button_, 152, 44);
  lv_obj_set_size(volume_down_, 58, 58);
  lv_obj_set_size(volume_slider_, 0, 14);
  lv_obj_set_flex_grow(volume_slider_, 1);
  configure_slider(volume_slider_, theme);
  lv_obj_set_size(volume_up_, 58, 58);
  lv_slider_set_range(volume_slider_, 0, 10);
  lv_obj_add_event_cb(volume_slider_, &DisplayAudioPage::handle_event,
                      LV_EVENT_VALUE_CHANGED, this);

  lv_obj_t* status = lv_obj_create(root_);
  if (status == nullptr) {
    destroy();
    return false;
  }
  theme.apply_status(status);
  lv_obj_set_size(status, LV_PCT(100), 48);
  lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);
  status_label_ = create_label(
      status, context_.locale, generated::TextId::display_audio_status_ready,
      theme, hmi::ui::TextRole::kAccent);
  if (status_label_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_align(status_label_, LV_ALIGN_LEFT_MID, 16, 0);
  update_view();
  return true;
}

void DisplayAudioPage::handle_event(lv_event_t* event) {
  if (event == nullptr) {
    return;
  }
  auto* self = static_cast<DisplayAudioPage*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  lv_obj_t* target = lv_event_get_target(event);
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_VALUE_CHANGED) {
    if (target == self->brightness_slider_) {
      self->set_brightness(
          static_cast<std::uint8_t>(lv_slider_get_value(target)));
    } else if (target == self->volume_slider_) {
      self->set_volume(static_cast<std::uint8_t>(lv_slider_get_value(target)));
    }
    return;
  }
  if (code != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  if (target == self->back_button_) {
    self->context_.request_dashboard(self->context_.user_data);
  } else if (target == self->brightness_down_ && self->state_.brightness > 1U) {
    self->set_brightness(self->state_.brightness - 1U);
  } else if (target == self->brightness_up_ && self->state_.brightness < 10U) {
    self->set_brightness(self->state_.brightness + 1U);
  } else if (target == self->volume_down_ && self->state_.volume > 0U) {
    self->set_volume(self->state_.volume - 1U);
  } else if (target == self->volume_up_ && self->state_.volume < 10U) {
    self->set_volume(self->state_.volume + 1U);
  } else if (target == self->preview_button_) {
    self->play_preview();
  }
}

void DisplayAudioPage::handle_locale_ko_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<DisplayAudioPage*>(lv_event_get_user_data(event));
  if (self != nullptr) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::ko_kr);
  }
}

void DisplayAudioPage::handle_locale_en_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<DisplayAudioPage*>(lv_event_get_user_data(event));
  if (self != nullptr) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::en_us);
  }
}

void DisplayAudioPage::handle_theme_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<DisplayAudioPage*>(lv_event_get_user_data(event));
  if (self != nullptr) {
    self->context_.request_theme_toggle(self->context_.user_data);
  }
}

hmi::settings::LevelSettingsResult DisplayAudioPage::set_brightness(
    std::uint8_t level) noexcept {
  const auto result = settings_.set_brightness(level);
  state_.brightness = settings_.brightness();
  accept_result(result);
  update_view();
  return result;
}

hmi::settings::LevelSettingsResult DisplayAudioPage::set_volume(
    std::uint8_t level) noexcept {
  const auto result = settings_.set_volume(level);
  state_.volume = settings_.volume();
  accept_result(result);
  update_view();
  return result;
}

hmi::settings::LevelSettingsResult DisplayAudioPage::play_preview() noexcept {
  const auto result = settings_.play_preview();
  accept_result(result);
  update_view();
  return result;
}

void DisplayAudioPage::accept_result(
    hmi::settings::LevelSettingsResult result) noexcept {
  using hmi::settings::LevelSettingsResult;
  switch (result) {
    case LevelSettingsResult::ok:
      state_.settings_status = SettingsPageStatus::kSaved;
      break;
    case LevelSettingsResult::apply_failed:
      state_.settings_status = SettingsPageStatus::kApplyFailed;
      break;
    case LevelSettingsResult::save_failed:
      state_.settings_status = SettingsPageStatus::kSaveFailed;
      break;
    case LevelSettingsResult::preview_unavailable:
      state_.settings_status = SettingsPageStatus::kPreviewUnavailable;
      break;
    case LevelSettingsResult::unavailable:
    case LevelSettingsResult::invalid:
      state_.settings_status = SettingsPageStatus::kUnavailable;
      break;
    case LevelSettingsResult::unchanged:
      state_.settings_status = SettingsPageStatus::kReady;
      break;
  }
}

void DisplayAudioPage::update_view() noexcept {
  if (brightness_slider_ != nullptr) {
    lv_slider_set_value(brightness_slider_, state_.brightness, LV_ANIM_OFF);
  }
  if (volume_slider_ != nullptr) {
    lv_slider_set_value(volume_slider_, state_.volume, LV_ANIM_OFF);
  }
  std::array<char, 64U> buffer{};
  const hmi::i18n::FormatArgument brightness_argument =
      hmi::i18n::FormatArgument::number("level", state_.brightness);
  if (brightness_value_ != nullptr &&
      hmi::i18n::format_to(
          generated::text(
              context_.locale,
              generated::TextId::display_audio_brightness_value),
          &brightness_argument, 1U, buffer.data(), buffer.size())
              .status == hmi::i18n::FormatStatus::ok) {
    lv_label_set_text(brightness_value_, buffer.data());
  }
  const hmi::i18n::FormatArgument volume_argument =
      hmi::i18n::FormatArgument::number("level", state_.volume);
  if (volume_value_ != nullptr &&
      hmi::i18n::format_to(
          generated::text(context_.locale,
                          generated::TextId::display_audio_volume_value),
          &volume_argument, 1U, buffer.data(), buffer.size())
              .status == hmi::i18n::FormatStatus::ok) {
    lv_label_set_text(volume_value_, buffer.data());
  }
  if (status_label_ != nullptr) {
    const generated::TextId status_id = status_text(state_.settings_status);
    lv_label_set_text(
        status_label_, generated::text(context_.locale, status_id).data());
    // 오류 문구는 별도 fixed-font glyph set을 사용하므로 상태 전환 때 font도 바꾼다.
    apply_fixed_font(status_label_, status_id);
  }
}

PageSnapshot DisplayAudioPage::snapshot() const noexcept { return state_; }

void DisplayAudioPage::destroy() noexcept {
  settings_.end();
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  back_button_ = nullptr;
  brightness_slider_ = nullptr;
  brightness_value_ = nullptr;
  brightness_down_ = nullptr;
  brightness_up_ = nullptr;
  volume_slider_ = nullptr;
  volume_value_ = nullptr;
  volume_down_ = nullptr;
  volume_up_ = nullptr;
  preview_button_ = nullptr;
  status_label_ = nullptr;
  context_ = {};
}

DisplayAudioPage::~DisplayAudioPage() { destroy(); }

}  // namespace hmi::samples::suite
