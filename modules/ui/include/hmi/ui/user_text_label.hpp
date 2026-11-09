#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lvgl.h>

#include "hmi/ui/font_repository.hpp"

namespace hmi::ui {

enum class UserTextStatus : std::uint8_t {
  ok,
  missing_glyph,
  invalid_utf8,
  font_unavailable,
  segment_limit,
  not_created,
};

/**
 * 서로 다른 분할 font가 섞인 UTF-8 문자열을 연속 LVGL label로 표시한다.
 *
 * widget은 parent가 소유하고 FontRepository는 이 객체와 widget보다 오래 살아야 한다.
 * 입력 문자열은 lv_label_set_text가 복사하므로 호출 후 원본 buffer를 유지할 필요가 없다.
 */
class UserTextLabel {
 public:
  static constexpr std::size_t kMaxSegments = 64U;
  static constexpr std::size_t kMaxUtf8Bytes = 192U;

  UserTextLabel() = default;
  UserTextLabel(const UserTextLabel&) = delete;
  UserTextLabel& operator=(const UserTextLabel&) = delete;
  ~UserTextLabel();

  bool create(lv_obj_t* parent, FontRepository& fonts) noexcept;
  void destroy() noexcept;
  [[nodiscard]] UserTextStatus set_text(std::string_view text) noexcept;
  [[nodiscard]] lv_obj_t* object() const noexcept { return root_; }
  [[nodiscard]] std::size_t segment_count() const noexcept { return segment_count_; }

 private:
  static void handle_delete(lv_event_t* event) noexcept;
  bool append_segment(const lv_font_t* font, const char* text) noexcept;

  lv_obj_t* root_ = nullptr;
  FontRepository* fonts_ = nullptr;
  std::size_t segment_count_ = 0U;
};

static_assert(sizeof(UserTextLabel) <= 32U,
              "UserTextLabel must not own user text or font bitmap copies");

}  // namespace hmi::ui

