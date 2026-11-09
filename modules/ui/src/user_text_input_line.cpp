#include "hmi/ui/user_text_input_line.hpp"

#include <algorithm>
#include <array>

#include "hmi/text/utf8.hpp"

namespace hmi::ui {
namespace {

constexpr lv_coord_t kHorizontalInset = 4;
constexpr lv_coord_t kCaretWidth = 2;
constexpr lv_coord_t kUnderlineHeight = 2;
constexpr lv_coord_t kMinimumGlyphWidth = 8;
constexpr lv_coord_t kDefaultLineHeight = 24;

void prepare_plain_object(lv_obj_t* object) noexcept {
  lv_obj_set_style_pad_all(object, 0, 0);
  lv_obj_set_style_border_width(object, 0, 0);
  lv_obj_set_style_radius(object, 0, 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace

UserTextInputLine::~UserTextInputLine() { destroy(); }

bool UserTextInputLine::create(lv_obj_t* parent, FontRepository& fonts,
                               lv_coord_t width, lv_coord_t height) noexcept {
  if (parent == nullptr || root_ != nullptr || !fonts.initialized() || width <= 0 ||
      height <= 0) {
    return false;
  }

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  fonts_ = &fonts;
  width_ = width;
  height_ = height;
  lv_obj_set_size(root_, width_, height_);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_radius(root_, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_scroll_dir(root_, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(root_, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(root_, &UserTextInputLine::handle_delete, LV_EVENT_DELETE, this);
  lv_obj_add_event_cb(root_, &UserTextInputLine::handle_pressed, LV_EVENT_PRESSED, this);

  blink_timer_ = lv_timer_create(&UserTextInputLine::handle_blink, kBlinkPeriodMs, this);
  if (blink_timer_ == nullptr) {
    destroy();
    return false;
  }
  lv_timer_pause(blink_timer_);

  if (render({}, 0U) != UserTextInputLineStatus::ok) {
    destroy();
    return false;
  }
  return true;
}

void UserTextInputLine::destroy() noexcept {
  if (blink_timer_ != nullptr) {
    lv_timer_del(blink_timer_);
    blink_timer_ = nullptr;
  }
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  content_ = nullptr;
  caret_ = nullptr;
  composition_underline_ = nullptr;
  fonts_ = nullptr;
  selection_handler_ = nullptr;
  selection_context_ = nullptr;
  display_codepoint_count_ = 0U;
  active_ = false;
  caret_visible_ = false;
}

void UserTextInputLine::handle_delete(lv_event_t* event) noexcept {
  auto* self = static_cast<UserTextInputLine*>(lv_event_get_user_data(event));
  if (self == nullptr || lv_event_get_target(event) != self->root_) {
    return;
  }
  if (self->blink_timer_ != nullptr) {
    lv_timer_del(self->blink_timer_);
    self->blink_timer_ = nullptr;
  }
  self->root_ = nullptr;
  self->content_ = nullptr;
  self->caret_ = nullptr;
  self->composition_underline_ = nullptr;
  self->fonts_ = nullptr;
  self->selection_handler_ = nullptr;
  self->selection_context_ = nullptr;
  self->display_codepoint_count_ = 0U;
  self->active_ = false;
  self->caret_visible_ = false;
}

void UserTextInputLine::handle_pressed(lv_event_t* event) noexcept {
  auto* self = static_cast<UserTextInputLine*>(lv_event_get_user_data(event));
  if (self == nullptr || self->root_ == nullptr) {
    return;
  }
  lv_indev_t* indev = lv_event_get_indev(event);
  if (indev == nullptr) {
    return;
  }
  lv_point_t point{};
  lv_indev_get_point(indev, &point);
  lv_area_t area{};
  lv_obj_get_coords(self->root_, &area);
  const lv_coord_t content_x = static_cast<lv_coord_t>(
      point.x - area.x1 + lv_obj_get_scroll_x(self->root_) - kHorizontalInset);
  const std::size_t selected = self->nearest_cursor_index(content_x);
  self->reset_blink();
  const auto handler = self->selection_handler_;
  void* const context = self->selection_context_;
  if (handler != nullptr) {
    handler(context, selected);
  }
}

void UserTextInputLine::handle_blink(lv_timer_t* timer) noexcept {
  auto* self = static_cast<UserTextInputLine*>(timer == nullptr ? nullptr : timer->user_data);
  if (self != nullptr && self->active_) {
    self->set_caret_visible(!self->caret_visible_);
  }
}

UserTextInputLineStatus UserTextInputLine::render(
    std::string_view committed, std::size_t cursor_codepoint_index,
    std::uint32_t composing_codepoint) noexcept {
  if (root_ == nullptr || fonts_ == nullptr) {
    return UserTextInputLineStatus::not_created;
  }
  if (committed.size() > kMaxUtf8Bytes) {
    return UserTextInputLineStatus::invalid_utf8;
  }
  const auto validation = hmi::text::utf8::validate(committed);
  if (validation.status != hmi::text::utf8::DecodeStatus::ok) {
    return UserTextInputLineStatus::invalid_utf8;
  }
  if (validation.codepoints > kMaxCommittedCodepoints) {
    return UserTextInputLineStatus::codepoint_limit;
  }
  if (cursor_codepoint_index > validation.codepoints) {
    return UserTextInputLineStatus::cursor_out_of_range;
  }
  const bool composing = composing_codepoint != 0U;
  if (composing && validation.codepoints >= kMaxDisplayCodepoints) {
    return UserTextInputLineStatus::codepoint_limit;
  }
  if (composing && hmi::text::utf8::encode(composing_codepoint).size == 0U) {
    return UserTextInputLineStatus::invalid_utf8;
  }

  auto walk_display = [&](auto&& visitor) noexcept -> bool {
    std::size_t byte_offset = 0U;
    std::size_t committed_index = 0U;
    std::size_t display_index = 0U;
    while (committed_index <= validation.codepoints) {
      if (composing && committed_index == cursor_codepoint_index) {
        if (!visitor(composing_codepoint, true, display_index++)) {
          return false;
        }
      }
      if (committed_index == validation.codepoints) {
        break;
      }
      const auto decoded = hmi::text::utf8::decode_next(committed, byte_offset);
      if (decoded.status != hmi::text::utf8::DecodeStatus::ok ||
          !visitor(decoded.codepoint, false, display_index++)) {
        return false;
      }
      byte_offset = decoded.next_offset;
      ++committed_index;
    }
    return true;
  };

  std::array<lv_coord_t, kMaxDisplayCodepoints + 1U> next_boundaries{};
  lv_coord_t max_line_height = kDefaultLineHeight;
  UserTextInputLineStatus overall = UserTextInputLineStatus::ok;
  UserTextInputLineStatus preflight = UserTextInputLineStatus::ok;
  const bool preflight_ok = walk_display(
      [&](std::uint32_t codepoint, bool, std::size_t display_index) noexcept {
        const FontResolution resolution = fonts_->resolve(codepoint);
        if (resolution.font == nullptr) {
          preflight = UserTextInputLineStatus::font_unavailable;
          return false;
        }
        if (resolution.status == FontResolveStatus::missing_glyph) {
          overall = UserTextInputLineStatus::missing_glyph;
        }
        lv_coord_t advance = static_cast<lv_coord_t>(lv_font_get_glyph_width(
            resolution.font, resolution.rendered_codepoint, 0U));
        if (advance < kMinimumGlyphWidth) {
          advance = std::max(kMinimumGlyphWidth,
                             static_cast<lv_coord_t>(resolution.font->line_height / 2));
        }
        next_boundaries[display_index + 1U] =
            static_cast<lv_coord_t>(next_boundaries[display_index] + advance);
        max_line_height = std::max(
            max_line_height, static_cast<lv_coord_t>(resolution.font->line_height));
        return true;
      });
  if (!preflight_ok) {
    return preflight;
  }

  lv_obj_t* next_content = lv_obj_create(root_);
  if (next_content == nullptr) {
    return UserTextInputLineStatus::object_unavailable;
  }
  prepare_plain_object(next_content);
  lv_obj_set_pos(next_content, kHorizontalInset, 0);
  const std::size_t next_count =
      validation.codepoints + static_cast<std::size_t>(composing);
  const lv_coord_t content_width = std::max(
      static_cast<lv_coord_t>(width_ - (2 * kHorizontalInset)),
      static_cast<lv_coord_t>(next_boundaries[next_count] + (2 * kHorizontalInset)));
  lv_obj_set_size(next_content, content_width, height_);

  UserTextInputLineStatus build_status = UserTextInputLineStatus::ok;
  const bool build_ok = walk_display(
      [&](std::uint32_t codepoint, bool, std::size_t display_index) noexcept {
        const FontResolution resolution = fonts_->resolve(codepoint);
        if (resolution.font == nullptr) {
          build_status = UserTextInputLineStatus::font_unavailable;
          return false;
        }
        const auto encoded = hmi::text::utf8::encode(resolution.rendered_codepoint);
        if (encoded.size == 0U) {
          build_status = UserTextInputLineStatus::invalid_utf8;
          return false;
        }
        std::array<char, 5U> glyph{};
        for (std::size_t index = 0U; index < encoded.size; ++index) {
          glyph[index] = encoded.bytes[index];
        }
        lv_obj_t* label = lv_label_create(next_content);
        if (label == nullptr) {
          build_status = UserTextInputLineStatus::object_unavailable;
          return false;
        }
        lv_label_set_text(label, glyph.data());
        lv_obj_set_style_text_font(label, resolution.font, 0);
        lv_obj_set_style_text_color(label, text_color_, 0);
        lv_obj_set_style_pad_all(label, 0, 0);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(label, next_boundaries[display_index],
                       static_cast<lv_coord_t>((height_ - resolution.font->line_height) / 2));
        return true;
      });
  if (!build_ok) {
    lv_obj_del(next_content);
    return build_status;
  }

  const std::size_t caret_boundary =
      cursor_codepoint_index + static_cast<std::size_t>(composing);
  const lv_coord_t next_caret_x = next_boundaries[caret_boundary];
  const lv_coord_t caret_height = std::min(
      static_cast<lv_coord_t>(height_ - 4), max_line_height);
  const lv_coord_t caret_y = static_cast<lv_coord_t>((height_ - caret_height) / 2);
  lv_obj_t* next_caret = lv_obj_create(next_content);
  if (next_caret == nullptr) {
    lv_obj_del(next_content);
    return UserTextInputLineStatus::object_unavailable;
  }
  prepare_plain_object(next_caret);
  lv_obj_set_size(next_caret, kCaretWidth, caret_height);
  lv_obj_set_pos(next_caret, next_caret_x, caret_y);
  lv_obj_set_style_bg_color(next_caret, accent_color_, 0);
  lv_obj_set_style_bg_opa(next_caret, LV_OPA_COVER, 0);

  lv_obj_t* next_underline = nullptr;
  if (composing) {
    next_underline = lv_obj_create(next_content);
    if (next_underline == nullptr) {
      lv_obj_del(next_content);
      return UserTextInputLineStatus::object_unavailable;
    }
    prepare_plain_object(next_underline);
    const lv_coord_t begin = next_boundaries[cursor_codepoint_index];
    const lv_coord_t underline_width = std::max(
        kCaretWidth,
        static_cast<lv_coord_t>(next_boundaries[cursor_codepoint_index + 1U] - begin));
    lv_obj_set_size(next_underline, underline_width, kUnderlineHeight);
    lv_obj_set_pos(next_underline, begin,
                   static_cast<lv_coord_t>(caret_y + caret_height));
    lv_obj_set_style_bg_color(next_underline, accent_color_, 0);
    lv_obj_set_style_bg_opa(next_underline, LV_OPA_COVER, 0);
  }

  lv_obj_t* const previous_content = content_;
  content_ = next_content;
  caret_ = next_caret;
  composition_underline_ = next_underline;
  boundaries_ = next_boundaries;
  display_codepoint_count_ = next_count;
  caret_x_ = next_caret_x;
  if (previous_content != nullptr) {
    lv_obj_del(previous_content);
  }
  set_caret_visible(active_);
  if (active_) {
    reset_blink();
  }
  lv_obj_update_layout(root_);
  ensure_caret_visible();
  return overall;
}

void UserTextInputLine::set_active(bool active) noexcept {
  active_ = active;
  if (blink_timer_ != nullptr) {
    if (active_) {
      lv_timer_resume(blink_timer_);
      reset_blink();
    } else {
      lv_timer_pause(blink_timer_);
      set_caret_visible(false);
    }
  }
}

void UserTextInputLine::reset_blink() noexcept {
  if (!active_) {
    return;
  }
  set_caret_visible(true);
  if (blink_timer_ != nullptr) {
    lv_timer_reset(blink_timer_);
  }
}

void UserTextInputLine::set_selection_handler(SelectionHandler handler,
                                              void* context) noexcept {
  selection_handler_ = handler;
  selection_context_ = context;
}

void UserTextInputLine::set_colors(lv_color_t text, lv_color_t accent) noexcept {
  text_color_ = text;
  accent_color_ = accent;
}

void UserTextInputLine::set_caret_visible(bool visible) noexcept {
  caret_visible_ = visible && active_ && caret_ != nullptr;
  if (caret_ != nullptr) {
    if (caret_visible_) {
      lv_obj_clear_flag(caret_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(caret_, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void UserTextInputLine::ensure_caret_visible() noexcept {
  if (root_ == nullptr) {
    return;
  }
  const lv_coord_t viewport = lv_obj_get_content_width(root_);
  const lv_coord_t caret = static_cast<lv_coord_t>(kHorizontalInset + caret_x_);
  lv_coord_t target = lv_obj_get_scroll_x(root_);
  if (caret < target + kHorizontalInset) {
    target = std::max<lv_coord_t>(0, static_cast<lv_coord_t>(caret - kHorizontalInset));
  } else if (caret + kCaretWidth > target + viewport - kHorizontalInset) {
    target = std::max<lv_coord_t>(
        0, static_cast<lv_coord_t>(caret + kCaretWidth - viewport + kHorizontalInset));
  }
  lv_obj_scroll_to_x(root_, target, LV_ANIM_OFF);
}

lv_coord_t UserTextInputLine::scroll_x() const noexcept {
  return root_ == nullptr ? 0 : lv_obj_get_scroll_x(root_);
}

std::size_t UserTextInputLine::nearest_cursor_index(lv_coord_t content_x) const noexcept {
  if (display_codepoint_count_ == 0U || content_x <= boundaries_[0]) {
    return 0U;
  }
  if (content_x >= boundaries_[display_codepoint_count_]) {
    return display_codepoint_count_;
  }
  for (std::size_t index = 0U; index < display_codepoint_count_; ++index) {
    const lv_coord_t midpoint = static_cast<lv_coord_t>(
        boundaries_[index] +
        (boundaries_[index + 1U] - boundaries_[index]) / 2);
    if (content_x < midpoint) {
      return index;
    }
  }
  return display_codepoint_count_;
}

}  // namespace hmi::ui
