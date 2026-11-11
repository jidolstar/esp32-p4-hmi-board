#include "pages/text_input/text_input_page.hpp"

#include <algorithm>
#include <array>

#include "components/page_header.hpp"
#include "hmi/i18n/format.hpp"
#include "hmi/text/utf8.hpp"
#include "hmi/ui/components.hpp"
#include "generated/fixed_fonts.hpp"
#include "generated/user_font_manifest.hpp"

namespace hmi::samples::suite {
namespace {

constexpr std::array<generated::TextId, 3U> kFieldLabels = {
    generated::TextId::text_input_field_name,
    generated::TextId::text_input_field_memo,
    generated::TextId::text_input_field_value};
constexpr std::array<generated::TextId, 3U> kFieldPlaceholders = {
    generated::TextId::text_input_field_name_placeholder,
    generated::TextId::text_input_field_memo_placeholder,
    generated::TextId::text_input_field_value_placeholder};

std::size_t field_index(TextFieldId field) noexcept {
  return static_cast<std::size_t>(field);
}

void configure_transparent_row(lv_obj_t* row, hmi::ui::UiTheme& theme,
                               lv_coord_t height) noexcept {
  theme.apply_transparent(row);
  lv_obj_set_size(row, LV_PCT(100), height);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
}

}  // namespace

TextInputPage::~TextInputPage() { destroy(); }

bool TextInputPage::create(lv_obj_t* parent, const PageContext& context) {
  if (parent == nullptr || root_ != nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.fonts.load == nullptr || context.fonts.free == nullptr) {
    return false;
  }
  context_ = context;
  if (!fonts_.initialize(generated::kUserFontManifest,
                         {this, &TextInputPage::load_font,
                          &TextInputPage::free_font})) {
    context_ = {};
    return false;
  }
  // 공통 glyph asset 하나만 먼저 확인한다. SD가 없거나 경로가 잘못돼도 page
  // 전환 자체는 유지하고 firmware 내장 font만 쓰는 복구 화면을 표시한다.
  const auto initial_font = fonts_.resolve(static_cast<std::uint32_t>('A'));
  if (initial_font.font == nullptr) {
    fonts_.reset();
    if (!create_font_recovery_layout(parent)) {
      destroy();
      return false;
    }
    return true;
  }
  font_assets_ready_ = true;
  if (!create_layout(parent)) {
    destroy();
    return false;
  }
  refresh_locale();
  refresh_fields();
  refresh_status();
  if (flow_state_ == TextInputFlowState::review) {
    lv_obj_clear_flag(review_scrim_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(review_scrim_);
  }
  return true;
}

bool TextInputPage::create_font_recovery_layout(lv_obj_t* parent) noexcept {
  hmi::ui::UiTheme& theme = *context_.theme;
  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  configure_page_shell_root(root_, theme);

  PageHeaderSpec header_spec;
  header_spec.title = generated::TextId::page_text_input_title;
  header_spec.locale = context_.locale;
  header_spec.back_event = &TextInputPage::handle_event;
  header_spec.locale_ko_event = &TextInputPage::handle_event;
  header_spec.locale_en_event = &TextInputPage::handle_event;
  header_spec.theme_event = &TextInputPage::handle_event;
  header_spec.user_data = this;
  const PageHeaderWidgets header =
      create_page_header(root_, header_spec, theme);
  if (!header) {
    return false;
  }
  back_button_ = header.back_button;
  locale_ko_button_ = header.locale_ko_button;
  locale_en_button_ = header.locale_en_button;
  theme_button_ = header.theme_button;

  lv_obj_t* panel = lv_obj_create(root_);
  if (panel == nullptr) {
    return false;
  }
  theme.apply_surface(panel);
  lv_obj_set_width(panel, LV_PCT(100));
  lv_obj_set_flex_grow(panel, 1);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(panel, 28, 0);
  lv_obj_set_style_pad_row(panel, 14, 0);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* heading = hmi::ui::create_label(
      panel, "SD font assets unavailable", theme, hmi::ui::TextRole::kTitle);
  lv_obj_t* detail = hmi::ui::create_label(
      panel, "Text Input needs its binary font assets from the SD card.", theme,
      hmi::ui::TextRole::kBody);
  lv_obj_t* action = hmi::ui::create_label(
      panel,
      "Copy dist/sd/hmi/ to the FAT32 SD card root, then restart the device.",
      theme, hmi::ui::TextRole::kMuted);
  if (heading == nullptr || detail == nullptr || action == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(detail, LV_PCT(100), 42, 3);
  hmi::ui::configure_multiline(action, LV_PCT(100), 64, 4);
  return true;
}

lv_font_t* TextInputPage::load_font(void* context,
                                    const char* relative_path,
                                    std::uint32_t size_bytes,
                                    std::uint32_t crc32) noexcept {
  auto* self = static_cast<TextInputPage*>(context);
  if (self == nullptr || self->context_.fonts.load == nullptr) {
    return nullptr;
  }
  return static_cast<lv_font_t*>(self->context_.fonts.load(
      self->context_.fonts.context, relative_path, size_bytes, crc32));
}

void TextInputPage::free_font(void* context, lv_font_t* font) noexcept {
  auto* self = static_cast<TextInputPage*>(context);
  if (self != nullptr && self->context_.fonts.free != nullptr) {
    self->context_.fonts.free(self->context_.fonts.context, font);
  }
}

bool TextInputPage::bind_fixed(lv_obj_t* label, TextId id) noexcept {
  if (label == nullptr || fixed_binding_count_ >= fixed_bindings_.size()) {
    return false;
  }
  fixed_bindings_[fixed_binding_count_++] = {label, id};
  return true;
}

lv_obj_t* TextInputPage::create_fixed_label(lv_obj_t* parent, TextId id,
                                            hmi::ui::TextRole role) noexcept {
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

lv_obj_t* TextInputPage::create_fixed_button(
    lv_obj_t* parent, TextId id, hmi::ui::ButtonVariant variant) noexcept {
  lv_obj_t* button = hmi::ui::create_button(
      parent, generated::text(context_.locale, id).data(), *context_.theme, variant,
      &TextInputPage::handle_event, this);
  if (button == nullptr) {
    return nullptr;
  }
  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label == nullptr || !bind_fixed(label, id)) {
    lv_obj_del(button);
    return nullptr;
  }
  lv_obj_set_style_text_font(label,
                             generated::fixed_font(generated::typography(id)), 0);
  return button;
}

bool TextInputPage::create_layout(lv_obj_t* parent) noexcept {
  hmi::ui::UiTheme& theme = *context_.theme;
  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  configure_page_shell_root(root_, theme);

  PageHeaderSpec header_spec;
  header_spec.title = TextId::page_text_input_title;
  header_spec.locale = context_.locale;
  header_spec.back_event = &TextInputPage::handle_event;
  header_spec.locale_ko_event = &TextInputPage::handle_event;
  header_spec.locale_en_event = &TextInputPage::handle_event;
  header_spec.theme_event = &TextInputPage::handle_event;
  header_spec.user_data = this;
  const PageHeaderWidgets header =
      create_page_header(root_, header_spec, theme);
  if (!header) {
    return false;
  }
  back_button_ = header.back_button;
  locale_ko_button_ = header.locale_ko_button;
  locale_en_button_ = header.locale_en_button;
  theme_button_ = header.theme_button;

  lv_obj_t* body = lv_obj_create(root_);
  if (body == nullptr) {
    return false;
  }
  theme.apply_transparent(body);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_style_pad_column(body, 12, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

  lv_obj_t* fields_panel = lv_obj_create(body);
  lv_obj_t* keyboard_panel = lv_obj_create(body);
  if (fields_panel == nullptr || keyboard_panel == nullptr) {
    return false;
  }
  theme.apply_surface(fields_panel);
  lv_obj_set_size(fields_panel, 350, LV_PCT(100));
  lv_obj_clear_flag(fields_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(fields_panel, 14, 0);
  lv_obj_set_style_pad_row(fields_panel, 10, 0);
  lv_obj_set_flex_flow(fields_panel, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* description = create_fixed_label(
      fields_panel, TextId::page_text_input_description, hmi::ui::TextRole::kMuted);
  if (description == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(description, LV_PCT(100), 48, 5);
  if (!create_fields(fields_panel)) {
    return false;
  }
  clear_button_ = create_fixed_button(
      fields_panel, TextId::text_input_action_clear,
      hmi::ui::ButtonVariant::kGhost);
  if (clear_button_ == nullptr) {
    return false;
  }
  lv_obj_set_size(clear_button_, LV_PCT(100), 42);

  theme.apply_surface(keyboard_panel);
  lv_obj_set_height(keyboard_panel, LV_PCT(100));
  lv_obj_set_flex_grow(keyboard_panel, 1);
  lv_obj_clear_flag(keyboard_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(keyboard_panel, 12, 0);
  lv_obj_set_style_pad_row(keyboard_panel, 8, 0);
  lv_obj_set_flex_flow(keyboard_panel, LV_FLEX_FLOW_COLUMN);
  // 확인 overlay는 page root의 flex/padding 영향을 받지 않도록 화면 content의
  // 직계 자식으로 둔다. 그래야 dim 영역과 dialog 중앙이 실제 화면 기준이 된다.
  return create_keyboard(keyboard_panel) && create_review_dialog(parent);
}

bool TextInputPage::create_fields(lv_obj_t* parent) noexcept {
  for (std::size_t index = 0U; index < kFieldCount; ++index) {
    lv_obj_t* field = lv_obj_create(parent);
    if (field == nullptr) {
      return false;
    }
    context_.theme->apply_card(field);
    lv_obj_set_size(field, LV_PCT(100), 92);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(field, &TextInputPage::handle_event, LV_EVENT_ALL, this);
    lv_obj_set_style_pad_all(field, 10, 0);
    lv_obj_set_style_pad_row(field, 5, 0);
    lv_obj_set_flex_flow(field, LV_FLEX_FLOW_COLUMN);
    field_buttons_[index] = field;

    if (create_fixed_label(field, kFieldLabels[index],
                           hmi::ui::TextRole::kMuted) == nullptr) {
      return false;
    }
    lv_obj_t* value_host = lv_obj_create(field);
    if (value_host == nullptr) {
      return false;
    }
    context_.theme->apply_transparent(value_host);
    lv_obj_set_size(value_host, LV_PCT(100), 38);
    lv_obj_clear_flag(value_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(value_host, 0, 0);

    placeholder_labels_[index] = create_fixed_label(
        value_host, kFieldPlaceholders[index], hmi::ui::TextRole::kMuted);
    cursor_bindings_[index] = {this, static_cast<std::uint8_t>(index)};
    if (placeholder_labels_[index] == nullptr ||
        !input_lines_[index].create(value_host, fonts_, 294, 38)) {
      return false;
    }
    input_lines_[index].set_selection_handler(
        &TextInputPage::handle_cursor_selection, &cursor_bindings_[index]);
    input_lines_[index].set_colors(context_.theme->text_color(),
                                   context_.theme->accent_color());
    lv_obj_align(placeholder_labels_[index], LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align(input_lines_[index].object(), LV_ALIGN_LEFT_MID, 0, 0);
  }
  return true;
}

bool TextInputPage::create_keyboard(lv_obj_t* parent) noexcept {
  lv_obj_t* status_panel = lv_obj_create(parent);
  if (status_panel == nullptr) {
    return false;
  }
  context_.theme->apply_status(status_panel);
  lv_obj_set_size(status_panel, LV_PCT(100), 64);
  lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(status_panel, 10, 0);

  status_label_ = hmi::ui::create_label(
      status_panel, "", *context_.theme, hmi::ui::TextRole::kBody);
  composing_label_ = create_fixed_label(
      status_panel, TextId::text_input_status_composing,
      hmi::ui::TextRole::kAccent);
  error_label_ = hmi::ui::create_label(
      status_panel, "", *context_.theme, hmi::ui::TextRole::kAccent);
  confirmed_label_ = create_fixed_label(
      status_panel, TextId::text_input_status_confirmed,
      hmi::ui::TextRole::kAccent);
  if (status_label_ == nullptr || composing_label_ == nullptr ||
      error_label_ == nullptr || confirmed_label_ == nullptr) {
    return false;
  }
  lv_obj_set_style_text_font(
      status_label_,
      generated::fixed_font(
          generated::typography(TextId::text_input_status_length)),
      0);
  lv_obj_set_style_text_font(
      error_label_,
      generated::fixed_font(
          generated::typography(TextId::text_input_error_character_limit)),
      0);
  lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_align(composing_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_align(error_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_align(confirmed_label_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_flag(confirmed_label_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_update_layout(parent);
  const lv_coord_t keyboard_width = lv_obj_get_content_width(parent);
  const lv_coord_t keyboard_height = static_cast<lv_coord_t>(
      lv_obj_get_content_height(parent) - lv_obj_get_height(status_panel) - 8);
  if (!keyboard_.create(
          parent, fonts_, *context_.theme,
          generated::fixed_font(
              generated::typography(TextId::text_input_keyboard_shift)),
          keyboard_width, keyboard_height)) {
    return false;
  }
  keyboard_.set_event_handler(&TextInputPage::handle_keyboard_event, this);
  return refresh_keyboard();
}

bool TextInputPage::create_review_dialog(lv_obj_t* parent) noexcept {
  review_scrim_ = lv_obj_create(parent);
  if (review_scrim_ == nullptr) {
    return false;
  }
  context_.theme->apply_scrim(review_scrim_);
  lv_obj_set_style_bg_opa(review_scrim_, LV_OPA_50, 0);
  lv_obj_set_size(review_scrim_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(review_scrim_, 0, 0);
  lv_obj_clear_flag(review_scrim_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(review_scrim_, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_CLICKABLE |
                                     LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* dialog = lv_obj_create(review_scrim_);
  if (dialog == nullptr) {
    return false;
  }
  context_.theme->apply_dialog(dialog);
  lv_obj_set_size(dialog, 620, 410);
  lv_obj_center(dialog);
  lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(dialog, 24, 0);
  lv_obj_set_style_pad_row(dialog, 12, 0);
  lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);

  if (create_fixed_label(dialog, TextId::text_input_review_title,
                         hmi::ui::TextRole::kTitle) == nullptr) {
    return false;
  }
  for (std::size_t index = 0U; index < kFieldCount; ++index) {
    lv_obj_t* row = lv_obj_create(dialog);
    if (row == nullptr) {
      return false;
    }
    context_.theme->apply_card(row);
    lv_obj_set_size(row, LV_PCT(100), 68);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* field_label = create_fixed_label(
        row, kFieldLabels[index], hmi::ui::TextRole::kMuted);
    lv_obj_t* value_host = lv_obj_create(row);
    if (field_label == nullptr || value_host == nullptr) {
      return false;
    }
    lv_obj_set_width(field_label, 100);
    context_.theme->apply_transparent(value_host);
    lv_obj_set_height(value_host, 38);
    lv_obj_set_flex_grow(value_host, 1);
    lv_obj_clear_flag(value_host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(value_host, 0, 0);

    review_empty_labels_[index] = create_fixed_label(
        value_host, TextId::text_input_review_empty,
        hmi::ui::TextRole::kMuted);
    if (review_empty_labels_[index] == nullptr ||
        !review_value_labels_[index].create(value_host, fonts_)) {
      return false;
    }
    context_.theme->apply_label(review_value_labels_[index].object(),
                                hmi::ui::TextRole::kAccent);
    lv_obj_align(review_empty_labels_[index], LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align(review_value_labels_[index].object(), LV_ALIGN_LEFT_MID, 0, 0);
  }

  lv_obj_t* actions = lv_obj_create(dialog);
  if (actions == nullptr) {
    return false;
  }
  configure_transparent_row(actions, *context_.theme, 52);
  lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  review_edit_button_ = create_fixed_button(
      actions, TextId::text_input_review_edit,
      hmi::ui::ButtonVariant::kGhost);
  review_confirm_button_ = create_fixed_button(
      actions, TextId::text_input_review_confirm,
      hmi::ui::ButtonVariant::kPrimary);
  if (review_edit_button_ == nullptr || review_confirm_button_ == nullptr) {
    return false;
  }
  lv_obj_set_size(review_edit_button_, 140, 48);
  lv_obj_set_size(review_confirm_button_, 160, 48);
  return true;
}

bool TextInputPage::refresh_keyboard() noexcept {
  if (keyboard_.object() == nullptr) {
    return false;
  }
  const TextId submit_id = active_index() + 1U < kFieldCount
                               ? TextId::text_input_keyboard_next
                               : TextId::text_input_keyboard_done;
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
      generated::text(context_.locale, submit_id).data(),
  };
  return keyboard_.render(active_model().mode(), active_model().shift(), captions);
}

void TextInputPage::refresh_review() noexcept {
  for (std::size_t index = 0U; index < kFieldCount; ++index) {
    const auto value = models_[index].editor().text();
    if (value.empty()) {
      lv_obj_clear_flag(review_empty_labels_[index], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(review_value_labels_[index].object(), LV_OBJ_FLAG_HIDDEN);
    } else {
      (void)review_value_labels_[index].set_text(value);
      lv_obj_add_flag(review_empty_labels_[index], LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(review_value_labels_[index].object(), LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void TextInputPage::handle_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<TextInputPage*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  lv_obj_t* target = lv_event_get_target(event);
  if (target == self->back_button_) {
    self->context_.request_dashboard(self->context_.user_data);
  } else if (target == self->theme_button_) {
    self->context_.request_theme_toggle(self->context_.user_data);
  } else if (target == self->locale_ko_button_) {
    self->set_locale(hmi::i18n::Locale::ko_kr);
  } else if (target == self->locale_en_button_) {
    self->set_locale(hmi::i18n::Locale::en_us);
  } else if (target == self->clear_button_) {
    self->clear_all();
  } else if (target == self->review_edit_button_) {
    self->edit_review();
  } else if (target == self->review_confirm_button_) {
    self->confirm_review();
  } else {
    for (std::size_t index = 0U; index < kFieldCount; ++index) {
      if (target == self->field_buttons_[index]) {
        self->select_field(static_cast<TextFieldId>(index));
        return;
      }
    }
  }
}

void TextInputPage::handle_keyboard_event(
    void* context, const hmi::ui::VirtualKeyEvent& event) noexcept {
  auto* self = static_cast<TextInputPage*>(context);
  if (self == nullptr) {
    return;
  }
  switch (event.action) {
    case hmi::ui::VirtualKeyAction::layout_character:
      self->press_character(event.layout_index);
      break;
    case hmi::ui::VirtualKeyAction::literal_character: {
      self->begin_editing();
      const auto status = self->active_model().insert_codepoint(event.codepoint);
      self->set_edit_status(status);
      break;
    }
    case hmi::ui::VirtualKeyAction::shift:
      self->toggle_shift();
      break;
    case hmi::ui::VirtualKeyAction::symbol:
      self->set_keyboard_mode(
          self->active_model().mode() == hmi::text::KeyboardMode::symbol
              ? self->last_letter_mode_
              : hmi::text::KeyboardMode::symbol);
      break;
    case hmi::ui::VirtualKeyAction::language:
      self->toggle_keyboard_language();
      break;
    case hmi::ui::VirtualKeyAction::space:
      self->insert_space();
      break;
    case hmi::ui::VirtualKeyAction::backspace:
      self->backspace();
      break;
    case hmi::ui::VirtualKeyAction::submit:
      self->complete();
      break;
  }
}

void TextInputPage::handle_cursor_selection(void* context,
                                            std::size_t display_index) noexcept {
  auto* binding = static_cast<CursorBinding*>(context);
  if (binding != nullptr && binding->page != nullptr) {
    binding->page->move_cursor_from_touch(binding->field_index, display_index);
  }
}

void TextInputPage::refresh_locale() noexcept {
  for (std::size_t index = 0U; index < fixed_binding_count_; ++index) {
    const FixedBinding& binding = fixed_bindings_[index];
    lv_label_set_text(binding.label,
                      generated::text(context_.locale, binding.id).data());
    lv_obj_set_style_text_font(
        binding.label, generated::fixed_font(generated::typography(binding.id)), 0);
  }
  (void)refresh_keyboard();
  refresh_review();
  refresh_status();
}

std::size_t TextInputPage::active_index() const noexcept {
  return field_index(active_field_);
}

hmi::text::InputModel& TextInputPage::active_model() noexcept {
  return models_[active_index()];
}

const hmi::text::InputModel& TextInputPage::active_model() const noexcept {
  return models_[active_index()];
}

void TextInputPage::refresh_field(std::size_t index) noexcept {
  const auto& model = models_[index];
  const auto text = model.editor().text();
  const bool active = index == active_index();
  const bool placeholder = text.empty() && !model.is_composing() && !active;
  input_lines_[index].set_colors(context_.theme->text_color(),
                                 context_.theme->accent_color());
  const std::uint32_t preview = active && model.is_composing()
                                    ? model.preview_codepoint()
                                    : 0U;
  (void)input_lines_[index].render(
      text, model.editor().cursor_codepoint_index(), preview);
  input_lines_[index].set_active(active && flow_state_ == TextInputFlowState::editing);
  lv_obj_set_style_border_width(field_buttons_[index], active ? 2 : 1, 0);
  if (active) {
    lv_obj_set_style_border_color(field_buttons_[index],
                                  context_.theme->accent_color(), 0);
  }
  if (placeholder) {
    lv_obj_clear_flag(placeholder_labels_[index], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(input_lines_[index].object(), LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_add_flag(placeholder_labels_[index], LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(input_lines_[index].object(), LV_OBJ_FLAG_HIDDEN);
}

void TextInputPage::refresh_fields() noexcept {
  for (std::size_t index = 0U; index < kFieldCount; ++index) {
    refresh_field(index);
  }
}

void TextInputPage::refresh_status() noexcept {
  if (status_label_ == nullptr) {
    return;
  }
  const auto& editor = active_model().editor();
  const std::array arguments = {
      hmi::i18n::FormatArgument::number(
          "count", static_cast<std::uint32_t>(editor.codepoint_count())),
      hmi::i18n::FormatArgument::number(
          "max", static_cast<std::uint32_t>(hmi::text::Editor::kMaxCodepoints)),
      hmi::i18n::FormatArgument::number(
          "bytes", static_cast<std::uint32_t>(editor.byte_size())),
  };
  std::array<char, 128U> buffer{};
  const auto formatted = hmi::i18n::format_to(
      generated::text(context_.locale, TextId::text_input_status_length),
      arguments.data(),
      arguments.size(), buffer.data(), buffer.size());
  lv_label_set_text(status_label_, formatted.status == hmi::i18n::FormatStatus::ok
                                        ? buffer.data()
                                        : "");

  if (flow_state_ == TextInputFlowState::confirmed) {
    lv_obj_clear_flag(confirmed_label_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(confirmed_label_, LV_OBJ_FLAG_HIDDEN);
  }

  if (flow_state_ == TextInputFlowState::editing && active_model().is_composing() &&
      !has_error_) {
    lv_obj_clear_flag(composing_label_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(composing_label_, LV_OBJ_FLAG_HIDDEN);
  }
  if (!has_error_) {
    lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  const std::array error_arguments = {hmi::i18n::FormatArgument::number(
      "max", static_cast<std::uint32_t>(
                 error_text_ == TextId::text_input_error_byte_limit
                     ? hmi::text::Editor::kMaxBytes
                     : hmi::text::Editor::kMaxCodepoints))};
  const auto error = hmi::i18n::format_to(
      generated::text(context_.locale, error_text_), error_arguments.data(),
      error_arguments.size(), buffer.data(), buffer.size());
  lv_label_set_text(error_label_,
                    error.status == hmi::i18n::FormatStatus::ok ? buffer.data() : "");
  lv_obj_clear_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
}

void TextInputPage::set_edit_status(hmi::text::EditStatus status) noexcept {
  if (status == hmi::text::EditStatus::ok) {
    begin_editing();
  }
  has_error_ = status != hmi::text::EditStatus::ok;
  if (status == hmi::text::EditStatus::byte_limit ||
      status == hmi::text::EditStatus::invalid_utf8 ||
      status == hmi::text::EditStatus::invalid_codepoint) {
    error_text_ = TextId::text_input_error_byte_limit;
  } else {
    error_text_ = TextId::text_input_error_character_limit;
  }
  // 일반 key 입력은 활성 line만 바뀐다. 비활성 field까지 다시 만들면 글자마다
  // 불필요한 LVGL object churn이 생기므로 field 전환 경로에서만 전체를 갱신한다.
  refresh_field(active_index());
  refresh_status();
}

void TextInputPage::begin_editing() noexcept {
  flow_state_ = TextInputFlowState::editing;
  if (review_scrim_ != nullptr) {
    lv_obj_add_flag(review_scrim_, LV_OBJ_FLAG_HIDDEN);
  }
}

bool TextInputPage::show_review() noexcept {
  for (auto& model : models_) {
    const auto status = model.complete();
    if (status != hmi::text::EditStatus::ok) {
      set_edit_status(status);
      return false;
    }
  }
  has_error_ = false;
  flow_state_ = TextInputFlowState::review;
  refresh_review();
  lv_obj_clear_flag(review_scrim_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(review_scrim_);
  refresh_fields();
  refresh_status();
  return true;
}

bool TextInputPage::move_cursor_from_touch(std::size_t field_index,
                                           std::size_t display_index) noexcept {
  if (!font_assets_ready_ || field_index >= kFieldCount ||
      flow_state_ == TextInputFlowState::review) {
    return false;
  }
  if (field_index != active_index() &&
      !select_field(static_cast<TextFieldId>(field_index))) {
    return false;
  }
  begin_editing();
  const auto status = models_[field_index].move_cursor_to(display_index);
  set_edit_status(status);
  return status == hmi::text::EditStatus::ok;
}

bool TextInputPage::select_field(TextFieldId field) noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  const std::size_t index = field_index(field);
  if (index >= models_.size()) {
    return false;
  }
  const auto committed = active_model().complete();
  if (committed != hmi::text::EditStatus::ok) {
    set_edit_status(committed);
    return false;
  }
  active_field_ = field;
  (void)active_model().move_cursor_to(active_model().editor().codepoint_count());
  begin_editing();
  has_error_ = false;
  (void)refresh_keyboard();
  refresh_fields();
  refresh_status();
  return true;
}

bool TextInputPage::set_locale(hmi::i18n::Locale locale) noexcept {
  if (!context_.set_locale(context_.user_data, locale)) {
    return false;
  }
  context_.locale = locale;
  refresh_locale();
  return true;
}

bool TextInputPage::press_character(std::size_t key_index) noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  begin_editing();
  const bool consume_shift = active_model().shift();
  const auto status = active_model().press_key(key_index);
  if (status == hmi::text::EditStatus::ok && consume_shift) {
    active_model().toggle_shift();
  }
  set_edit_status(status);
  // 202609와 같은 hot path: 일반 문자는 기존 key tree를 유지하고, 한 글자용
  // Shift가 실제 소비돼 key caption이 바뀔 때만 keyboard를 다시 구성한다.
  if (status == hmi::text::EditStatus::ok && consume_shift) {
    (void)refresh_keyboard();
  }
  return status == hmi::text::EditStatus::ok;
}

bool TextInputPage::set_keyboard_mode(hmi::text::KeyboardMode mode) noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  const auto status = active_model().set_mode(mode);
  if (status != hmi::text::EditStatus::ok) {
    set_edit_status(status);
    return false;
  }
  if (mode == hmi::text::KeyboardMode::hangul ||
      mode == hmi::text::KeyboardMode::latin) {
    last_letter_mode_ = mode;
  }
  begin_editing();
  has_error_ = false;
  const bool rebuilt = refresh_keyboard();
  refresh_fields();
  refresh_status();
  return rebuilt;
}

bool TextInputPage::toggle_keyboard_language() noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  const auto mode = active_model().mode();
  hmi::text::KeyboardMode next = last_letter_mode_;
  if (mode == hmi::text::KeyboardMode::hangul) {
    next = hmi::text::KeyboardMode::latin;
  } else if (mode == hmi::text::KeyboardMode::latin) {
    next = hmi::text::KeyboardMode::hangul;
  }
  return set_keyboard_mode(next);
}

bool TextInputPage::toggle_shift() noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  active_model().toggle_shift();
  return refresh_keyboard();
}

bool TextInputPage::backspace() noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  begin_editing();
  const auto status = active_model().backspace();
  set_edit_status(status);
  return status == hmi::text::EditStatus::ok;
}

bool TextInputPage::insert_space() noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  begin_editing();
  const auto status = active_model().insert_space();
  set_edit_status(status);
  return status == hmi::text::EditStatus::ok;
}

bool TextInputPage::complete() noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  const auto status = active_model().complete();
  if (status != hmi::text::EditStatus::ok) {
    set_edit_status(status);
    return false;
  }
  if (active_index() + 1U < kFieldCount) {
    active_field_ = static_cast<TextFieldId>(active_index() + 1U);
    (void)active_model().move_cursor_to(active_model().editor().codepoint_count());
    begin_editing();
    has_error_ = false;
    (void)refresh_keyboard();
    refresh_fields();
    refresh_status();
    return true;
  }
  return show_review();
}

bool TextInputPage::edit_review() noexcept {
  if (!font_assets_ready_ || flow_state_ != TextInputFlowState::review) {
    return false;
  }
  begin_editing();
  (void)active_model().move_cursor_to(active_model().editor().codepoint_count());
  refresh_fields();
  refresh_status();
  return true;
}

bool TextInputPage::confirm_review() noexcept {
  if (!font_assets_ready_ || flow_state_ != TextInputFlowState::review) {
    return false;
  }
  lv_obj_add_flag(review_scrim_, LV_OBJ_FLAG_HIDDEN);
  flow_state_ = TextInputFlowState::confirmed;
  has_error_ = false;
  refresh_fields();
  refresh_status();
  return true;
}

void TextInputPage::clear_all() noexcept {
  if (!font_assets_ready_) {
    return;
  }
  for (auto& model : models_) {
    model.clear();
  }
  active_field_ = TextFieldId::name;
  begin_editing();
  has_error_ = false;
  (void)refresh_keyboard();
  refresh_fields();
  refresh_status();
}

bool TextInputPage::handle_utf8_input(const char* utf8) noexcept {
  if (!font_assets_ready_ || utf8 == nullptr) {
    return false;
  }
  const std::string_view text{utf8};
  begin_editing();
  const auto validation = hmi::text::utf8::validate(text);
  if (validation.status != hmi::text::utf8::DecodeStatus::ok) {
    set_edit_status(hmi::text::EditStatus::invalid_utf8);
    return false;
  }
  const auto committed = active_model().complete();
  if (committed != hmi::text::EditStatus::ok) {
    set_edit_status(committed);
    return false;
  }
  const auto& editor = active_model().editor();
  if (editor.byte_size() + validation.bytes > hmi::text::Editor::kMaxBytes) {
    set_edit_status(hmi::text::EditStatus::byte_limit);
    return false;
  }
  if (editor.codepoint_count() + validation.codepoints >
      hmi::text::Editor::kMaxCodepoints) {
    set_edit_status(hmi::text::EditStatus::codepoint_limit);
    return false;
  }
  std::size_t offset = 0U;
  while (offset < text.size()) {
    const auto decoded = hmi::text::utf8::decode_next(text, offset);
    const auto status = active_model().insert_codepoint(decoded.codepoint);
    if (status != hmi::text::EditStatus::ok) {
      set_edit_status(status);
      return false;
    }
    offset = decoded.next_offset;
  }
  set_edit_status(hmi::text::EditStatus::ok);
  return true;
}

bool TextInputPage::handle_key(hmi::profiles::AppKey key) noexcept {
  if (!font_assets_ready_) {
    return false;
  }
  if (key == hmi::profiles::AppKey::enter) {
    return complete();
  }
  begin_editing();
  hmi::text::EditStatus status = hmi::text::EditStatus::ok;
  switch (key) {
    case hmi::profiles::AppKey::backspace:
      status = active_model().backspace();
      break;
    case hmi::profiles::AppKey::enter:
      break;
    case hmi::profiles::AppKey::cursor_left:
      status = active_model().move_cursor(-1);
      break;
    case hmi::profiles::AppKey::cursor_right:
      status = active_model().move_cursor(1);
      break;
  }
  set_edit_status(status);
  return status == hmi::text::EditStatus::ok;
}

TextInputSnapshot TextInputPage::text_snapshot() const noexcept {
  const auto& model = active_model();
  return {context_.locale,
          model.mode(),
          active_field_,
          model.editor().codepoint_count(),
          model.editor().byte_size(),
          model.editor().cursor_codepoint_index(),
          model.is_composing(),
          flow_state_};
}

std::string_view TextInputPage::field_text(TextFieldId field) const noexcept {
  const std::size_t index = field_index(field);
  return index < models_.size() ? models_[index].editor().text()
                                : std::string_view{};
}

void TextInputPage::destroy() noexcept {
  // review_scrim_은 전체 화면 정렬을 위해 root_의 형제이므로 별도로 해제한다.
  if (review_scrim_ != nullptr) {
    lv_obj_del(review_scrim_);
  }
  review_scrim_ = nullptr;
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  keyboard_.destroy();
  for (auto& line : input_lines_) {
    line.destroy();
  }
  for (auto& label : review_value_labels_) {
    label.destroy();
  }
  // 모든 label이 삭제된 뒤에만 binary font backing data를 해제한다.
  fonts_.reset();
  field_buttons_.fill(nullptr);
  placeholder_labels_.fill(nullptr);
  review_empty_labels_.fill(nullptr);
  cursor_bindings_.fill({});
  fixed_bindings_.fill({});
  fixed_binding_count_ = 0U;
  context_ = {};
  back_button_ = nullptr;
  theme_button_ = nullptr;
  locale_ko_button_ = nullptr;
  locale_en_button_ = nullptr;
  clear_button_ = nullptr;
  status_label_ = nullptr;
  composing_label_ = nullptr;
  error_label_ = nullptr;
  confirmed_label_ = nullptr;
  review_edit_button_ = nullptr;
  review_confirm_button_ = nullptr;
  has_error_ = false;
  font_assets_ready_ = false;
}

}  // namespace hmi::samples::suite

