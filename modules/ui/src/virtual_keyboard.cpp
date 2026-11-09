#include "hmi/ui/virtual_keyboard.hpp"

#include <algorithm>
#include <array>

#include "hmi/text/utf8.hpp"

namespace hmi::ui {
namespace {

constexpr lv_coord_t kGap = 6;

void configure_row(lv_obj_t* row, UiTheme& theme, lv_coord_t width,
                   lv_coord_t height) noexcept {
  theme.apply_transparent(row);
  lv_obj_set_size(row, width, height);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, kGap, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
}

}  // namespace

VirtualKeyboard::~VirtualKeyboard() { destroy(); }

bool VirtualKeyboard::create(lv_obj_t* parent, FontRepository& fonts,
                             UiTheme& theme, const lv_font_t* action_font,
                             lv_coord_t width, lv_coord_t height) noexcept {
  if (parent == nullptr || root_ != nullptr || !fonts.initialized() ||
      !theme.initialized() || action_font == nullptr || width <= 0 || height <= 0) {
    return false;
  }
  const lv_coord_t row_height =
      static_cast<lv_coord_t>((height - kGap * (kRowCount - 1U)) / kRowCount);
  if (row_height < kMinimumKeyHeight) {
    return false;
  }

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  fonts_ = &fonts;
  theme_ = &theme;
  action_font_ = action_font;
  width_ = width;
  height_ = height;
  row_height_ = row_height;
  key_width_ = static_cast<lv_coord_t>((width_ - (9 * kGap)) / 10);
  theme_->apply_transparent(root_);
  lv_obj_set_size(root_, width_, height_);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_set_style_pad_row(root_, kGap, 0);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_event_cb(root_, &VirtualKeyboard::handle_event, LV_EVENT_DELETE, this);
  return true;
}

void VirtualKeyboard::destroy() noexcept {
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  rows_.fill(nullptr);
  keys_.fill({});
  fonts_ = nullptr;
  theme_ = nullptr;
  action_font_ = nullptr;
  handler_ = nullptr;
  handler_context_ = nullptr;
  key_count_ = 0U;
  width_ = 0;
  height_ = 0;
  row_height_ = 0;
  key_width_ = 0;
}

void VirtualKeyboard::handle_event(lv_event_t* event) noexcept {
  auto* self = static_cast<VirtualKeyboard*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_DELETE && lv_event_get_target(event) == self->root_) {
    self->root_ = nullptr;
    self->rows_.fill(nullptr);
    self->keys_.fill({});
    self->fonts_ = nullptr;
    self->theme_ = nullptr;
    self->action_font_ = nullptr;
    self->handler_ = nullptr;
    self->handler_context_ = nullptr;
    self->key_count_ = 0U;
    return;
  }
  if (code != LV_EVENT_CLICKED || self->handler_ == nullptr) {
    return;
  }
  lv_obj_t* const target = lv_event_get_target(event);
  for (std::size_t index = 0U; index < self->key_count_; ++index) {
    const KeySlot& slot = self->keys_[index];
    if (slot.button == target) {
      const VirtualKeyEvent value{slot.action, slot.codepoint, slot.layout_index};
      const auto handler = self->handler_;
      void* const context = self->handler_context_;
      handler(context, value);
      return;
    }
  }
}

bool VirtualKeyboard::create_rows() noexcept {
  for (std::size_t index = 0U; index < rows_.size(); ++index) {
    rows_[index] = lv_obj_create(root_);
    if (rows_[index] == nullptr) {
      return false;
    }
    configure_row(rows_[index], *theme_, width_, row_height_);
  }
  return true;
}

bool VirtualKeyboard::set_leading_inset(lv_obj_t* row,
                                        lv_coord_t width) noexcept {
  if (row == nullptr || width <= 0) {
    return width == 0;
  }
  // 빈 flex child는 첫 key 앞에 column gap을 하나 더 만들어 행을 오른쪽으로
  // 밀어낸다. row inset으로 시작점을 옮겨 계산된 폭 안에 마지막 key를 둔다.
  lv_obj_set_style_pad_left(row, width, 0);
  return true;
}

bool VirtualKeyboard::add_dynamic_key(lv_obj_t* row, std::uint8_t row_index,
                                      lv_coord_t width, VirtualKeyAction action,
                                      std::uint32_t codepoint,
                                      std::size_t layout_index,
                                      ButtonVariant variant) noexcept {
  if (key_count_ >= keys_.size() || layout_index > 255U) {
    return false;
  }
  const FontResolution resolution = fonts_->resolve(codepoint);
  if (resolution.font == nullptr) {
    return false;
  }
  const auto encoded = hmi::text::utf8::encode(resolution.rendered_codepoint);
  if (encoded.size == 0U) {
    return false;
  }
  std::array<char, 5U> glyph{};
  for (std::size_t index = 0U; index < encoded.size; ++index) {
    glyph[index] = encoded.bytes[index];
  }

  lv_obj_t* button = lv_btn_create(row);
  lv_obj_t* label = button == nullptr ? nullptr : lv_label_create(button);
  if (button == nullptr || label == nullptr) {
    if (button != nullptr) {
      lv_obj_del(button);
    }
    return false;
  }
  theme_->apply_button(button, variant);
  lv_obj_set_size(button, width, row_height_);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_add_event_cb(button, &VirtualKeyboard::handle_event, LV_EVENT_CLICKED, this);
  lv_label_set_text(label, glyph.data());
  theme_->apply_label(label, variant == ButtonVariant::kPrimary ? TextRole::kInverse
                                                               : TextRole::kBody);
  lv_obj_set_style_text_font(label, resolution.font, 0);
  lv_obj_center(label);
  keys_[key_count_++] = {button, codepoint, static_cast<std::uint8_t>(layout_index),
                         row_index, action};
  return true;
}

bool VirtualKeyboard::add_action_key(lv_obj_t* row, std::uint8_t row_index,
                                     lv_coord_t width, VirtualKeyAction action,
                                     const char* caption,
                                     ButtonVariant variant) noexcept {
  if (key_count_ >= keys_.size() || caption == nullptr) {
    return false;
  }
  lv_obj_t* button = lv_btn_create(row);
  lv_obj_t* label = button == nullptr ? nullptr : lv_label_create(button);
  if (button == nullptr || label == nullptr) {
    if (button != nullptr) {
      lv_obj_del(button);
    }
    return false;
  }
  theme_->apply_button(button, variant);
  lv_obj_set_size(button, width, row_height_);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(button, 2, 0);
  lv_obj_add_event_cb(button, &VirtualKeyboard::handle_event, LV_EVENT_CLICKED, this);
  lv_label_set_text(label, caption);
  theme_->apply_label(label, variant == ButtonVariant::kPrimary ? TextRole::kInverse
                                                               : TextRole::kBody);
  lv_obj_set_style_text_font(label, action_font_, 0);
  lv_obj_center(label);
  keys_[key_count_++] = {button, 0U, 0U, row_index, action};
  return true;
}

bool VirtualKeyboard::add_layout_row(
    std::uint8_t visual_row, const hmi::text::KeyboardLayoutView& layout,
    std::size_t descriptor_index, bool include_shift, bool include_submit,
    const VirtualKeyboardCaptions& captions, bool shift_active) noexcept {
  if (visual_row >= rows_.size() || descriptor_index >= layout.row_count) {
    return false;
  }
  const hmi::text::KeyboardRowDescriptor descriptor = layout.rows[descriptor_index];
  if (descriptor.key_offset + descriptor.key_count > layout.size) {
    return false;
  }
  lv_obj_t* const row = rows_[visual_row];
  lv_coord_t shift_width =
      include_shift ? static_cast<lv_coord_t>(key_width_ * 3 / 2) : 0;
  lv_coord_t submit_width =
      include_submit ? static_cast<lv_coord_t>(key_width_ * 17 / 10) : 0;
  if (include_shift && include_submit) {
    // Shift·문자·submit을 고정 배율로 더하면 행 폭을 넘을 수
    // 있으므로, 문자 key와 gap을 먼저 빼고 남은 폭을 나눈다.
    const lv_coord_t action_width = static_cast<lv_coord_t>(
        width_ - descriptor.key_count * key_width_ -
        (descriptor.key_count + 1U) * kGap);
    if (action_width < 2 * kMinimumKeyHeight) {
      return false;
    }
    shift_width = static_cast<lv_coord_t>(action_width * 15 / 32);
    submit_width = static_cast<lv_coord_t>(action_width - shift_width);
  }
  if (include_shift) {
    if (!add_action_key(row, visual_row, shift_width, VirtualKeyAction::shift,
                        captions.shift,
                        shift_active ? ButtonVariant::kPrimary
                                     : ButtonVariant::kGhost)) {
      return false;
    }
  } else {
    const lv_coord_t content_width = static_cast<lv_coord_t>(
        descriptor.key_count * key_width_ +
        (descriptor.key_count > 0U ? (descriptor.key_count - 1U) * kGap : 0U) +
        (include_submit ? submit_width + kGap : 0));
    const lv_coord_t leading = std::max<lv_coord_t>(
        static_cast<lv_coord_t>((key_width_ + kGap) * descriptor.leading_half_units / 2),
        static_cast<lv_coord_t>((width_ - content_width) / 2));
    if (!set_leading_inset(row, std::max<lv_coord_t>(0, leading))) {
      return false;
    }
  }

  for (std::size_t local = 0U; local < descriptor.key_count; ++local) {
    const std::size_t layout_index = descriptor.key_offset + local;
    if (!add_dynamic_key(row, visual_row, key_width_,
                         VirtualKeyAction::layout_character,
                         layout.keys[layout_index].codepoint, layout_index)) {
      return false;
    }
  }
  return !include_submit ||
         add_action_key(row, visual_row, submit_width, VirtualKeyAction::submit,
                        captions.submit, ButtonVariant::kPrimary);
}

bool VirtualKeyboard::render(hmi::text::KeyboardMode mode, bool shift,
                             const VirtualKeyboardCaptions& captions) noexcept {
  if (root_ == nullptr || fonts_ == nullptr || theme_ == nullptr ||
      captions.shift == nullptr || captions.symbol == nullptr ||
      captions.language == nullptr || captions.space == nullptr ||
      captions.backspace == nullptr || captions.submit == nullptr) {
    return false;
  }
  lv_obj_clean(root_);
  rows_.fill(nullptr);
  keys_.fill({});
  key_count_ = 0U;
  if (!create_rows()) {
    return false;
  }

  const auto numbers = hmi::text::keyboard_layout(hmi::text::KeyboardMode::number, false);
  const lv_coord_t number_row_width = static_cast<lv_coord_t>(
      numbers.size * key_width_ + (numbers.size - 1U) * kGap);
  if (!set_leading_inset(
          rows_[0],
          std::max<lv_coord_t>(0, static_cast<lv_coord_t>(
                                     (width_ - number_row_width) / 2)))) {
    return false;
  }
  for (std::size_t index = 0U; index < numbers.size; ++index) {
    if (!add_dynamic_key(rows_[0], 0U, key_width_,
                         VirtualKeyAction::literal_character,
                         numbers.keys[index].codepoint, index)) {
      return false;
    }
  }

  if (mode != hmi::text::KeyboardMode::number) {
    const auto layout = hmi::text::keyboard_layout(mode, shift);
    if (layout.row_count != 3U ||
        !add_layout_row(1U, layout, 0U, false, false, captions, shift) ||
        !add_layout_row(2U, layout, 1U, false, false, captions, shift) ||
        !add_layout_row(3U, layout, 2U,
                        mode != hmi::text::KeyboardMode::symbol, true,
                        captions, shift)) {
      return false;
    }
  } else {
    if (!set_leading_inset(
            rows_[3], static_cast<lv_coord_t>(width_ - key_width_ * 2)) ||
        !add_action_key(rows_[3], 3U, static_cast<lv_coord_t>(key_width_ * 2),
                        VirtualKeyAction::submit, captions.submit,
                        ButtonVariant::kPrimary)) {
      return false;
    }
  }

  constexpr std::array<int, 6U> weights = {13, 13, 8, 36, 8, 18};
  const lv_coord_t available = static_cast<lv_coord_t>(width_ - 5 * kGap);
  std::array<lv_coord_t, weights.size()> widths{};
  lv_coord_t allocated = 0;
  for (std::size_t index = 0U; index + 1U < weights.size(); ++index) {
    widths[index] = static_cast<lv_coord_t>(available * weights[index] / 96);
    allocated = static_cast<lv_coord_t>(allocated + widths[index]);
  }
  widths.back() = static_cast<lv_coord_t>(available - allocated);
  const bool symbol_active = mode == hmi::text::KeyboardMode::symbol;
  return add_action_key(rows_[4], 4U, widths[0], VirtualKeyAction::symbol,
                        captions.symbol,
                        symbol_active ? ButtonVariant::kPrimary
                                      : ButtonVariant::kGhost) &&
         add_action_key(rows_[4], 4U, widths[1], VirtualKeyAction::language,
                        captions.language, ButtonVariant::kGhost) &&
         add_dynamic_key(rows_[4], 4U, widths[2],
                         VirtualKeyAction::literal_character, ',', 0U,
                         ButtonVariant::kGhost) &&
         add_action_key(rows_[4], 4U, widths[3], VirtualKeyAction::space,
                        captions.space, ButtonVariant::kSecondary) &&
         add_dynamic_key(rows_[4], 4U, widths[4],
                         VirtualKeyAction::literal_character, '.', 0U,
                         ButtonVariant::kGhost) &&
         add_action_key(rows_[4], 4U, widths[5], VirtualKeyAction::backspace,
                        captions.backspace, ButtonVariant::kGhost);
}

void VirtualKeyboard::set_event_handler(EventHandler handler,
                                        void* context) noexcept {
  handler_ = handler;
  handler_context_ = context;
}

VirtualKeyEvent VirtualKeyboard::key_event(std::size_t index) const noexcept {
  if (index >= key_count_) {
    return {};
  }
  const KeySlot& slot = keys_[index];
  return {slot.action, slot.codepoint, slot.layout_index};
}

std::uint8_t VirtualKeyboard::key_row(std::size_t index) const noexcept {
  return index < key_count_ ? keys_[index].row : 0xFFU;
}

lv_obj_t* VirtualKeyboard::key_object(std::size_t index) const noexcept {
  return index < key_count_ ? keys_[index].button : nullptr;
}

}  // namespace hmi::ui
