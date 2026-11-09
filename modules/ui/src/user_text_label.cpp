#include "hmi/ui/user_text_label.hpp"

#include <array>
#include <cstring>

#include "hmi/text/utf8.hpp"

namespace hmi::ui {

UserTextLabel::~UserTextLabel() { destroy(); }

bool UserTextLabel::create(lv_obj_t* parent, FontRepository& fonts) noexcept {
  if (parent == nullptr || root_ != nullptr || !fonts.initialized()) {
    return false;
  }
  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  fonts_ = &fonts;
  lv_obj_add_event_cb(root_, &UserTextLabel::handle_delete, LV_EVENT_DELETE, this);
  lv_obj_set_size(root_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_set_style_pad_column(root_, 0, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  return true;
}

void UserTextLabel::destroy() noexcept {
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  fonts_ = nullptr;
  segment_count_ = 0U;
}

void UserTextLabel::handle_delete(lv_event_t* event) noexcept {
  auto* self = static_cast<UserTextLabel*>(lv_event_get_user_data(event));
  if (self != nullptr && lv_event_get_target(event) == self->root_) {
    self->root_ = nullptr;
    self->fonts_ = nullptr;
    self->segment_count_ = 0U;
  }
}

bool UserTextLabel::append_segment(const lv_font_t* font,
                                   const char* text) noexcept {
  if (segment_count_ >= kMaxSegments || font == nullptr || text == nullptr) {
    return false;
  }
  lv_obj_t* label = lv_label_create(root_);
  if (label == nullptr) {
    return false;
  }
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_pad_all(label, 0, 0);
  ++segment_count_;
  return true;
}

UserTextStatus UserTextLabel::set_text(std::string_view text) noexcept {
  if (root_ == nullptr || fonts_ == nullptr) {
    return UserTextStatus::not_created;
  }
  if (text.size() > kMaxUtf8Bytes ||
      hmi::text::utf8::validate(text).status != hmi::text::utf8::DecodeStatus::ok) {
    return UserTextStatus::invalid_utf8;
  }

  lv_obj_clean(root_);
  segment_count_ = 0U;
  std::array<char, kMaxUtf8Bytes + 1U> segment{};
  std::size_t segment_bytes = 0U;
  const lv_font_t* segment_font = nullptr;
  UserTextStatus overall = UserTextStatus::ok;

  auto flush = [&]() noexcept -> bool {
    if (segment_bytes == 0U) {
      return true;
    }
    segment[segment_bytes] = '\0';
    const bool appended = append_segment(segment_font, segment.data());
    segment_bytes = 0U;
    return appended;
  };

  std::size_t offset = 0U;
  while (offset < text.size()) {
    const auto decoded = hmi::text::utf8::decode_next(text, offset);
    if (decoded.status != hmi::text::utf8::DecodeStatus::ok) {
      lv_obj_clean(root_);
      segment_count_ = 0U;
      return UserTextStatus::invalid_utf8;
    }
    const FontResolution resolution = fonts_->resolve(decoded.codepoint);
    if (resolution.font == nullptr) {
      lv_obj_clean(root_);
      segment_count_ = 0U;
      return UserTextStatus::font_unavailable;
    }
    if (resolution.status == FontResolveStatus::missing_glyph) {
      overall = UserTextStatus::missing_glyph;
    }
    const auto encoded = hmi::text::utf8::encode(resolution.rendered_codepoint);
    if (encoded.size == 0U) {
      lv_obj_clean(root_);
      segment_count_ = 0U;
      return UserTextStatus::invalid_utf8;
    }
    if (segment_font != nullptr && segment_font != resolution.font && !flush()) {
      lv_obj_clean(root_);
      segment_count_ = 0U;
      return UserTextStatus::segment_limit;
    }
    segment_font = resolution.font;
    std::memcpy(segment.data() + segment_bytes, encoded.bytes.data(), encoded.size);
    segment_bytes += encoded.size;
    offset = decoded.next_offset;
  }
  if (!flush()) {
    lv_obj_clean(root_);
    segment_count_ = 0U;
    return UserTextStatus::segment_limit;
  }
  return overall;
}

}  // namespace hmi::ui

