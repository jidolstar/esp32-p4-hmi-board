#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lvgl.h>

#include "hmi/ui/font_repository.hpp"

namespace hmi::ui {

enum class UserTextInputLineStatus : std::uint8_t {
  ok,
  missing_glyph,
  invalid_utf8,
  codepoint_limit,
  cursor_out_of_range,
  font_unavailable,
  object_unavailable,
  not_created,
};

/**
 * 분할 사용자 글꼴 문자열 위에 실제 caret와 한글 조합 밑줄을 표시하는 입력줄이다.
 *
 * 문자열과 editor를 소유하지 않는다. touch 위치는 UTF-8 codepoint 경계 index로만
 * callback하며, FontRepository와 callback context는 이 객체보다 오래 살아야 한다.
 */
class UserTextInputLine {
 public:
  static constexpr std::size_t kMaxCommittedCodepoints = 64U;
  static constexpr std::size_t kMaxDisplayCodepoints = 65U;
  static constexpr std::size_t kMaxUtf8Bytes = 192U;
  static constexpr std::uint32_t kBlinkPeriodMs = 500U;

  using SelectionHandler = void (*)(void* context,
                                    std::size_t display_codepoint_index) noexcept;

  UserTextInputLine() = default;
  UserTextInputLine(const UserTextInputLine&) = delete;
  UserTextInputLine& operator=(const UserTextInputLine&) = delete;
  ~UserTextInputLine();

  bool create(lv_obj_t* parent, FontRepository& fonts, lv_coord_t width,
              lv_coord_t height) noexcept;
  void destroy() noexcept;

  /**
   * committed 문자열의 cursor 위치에 composing_codepoint를 preview한다.
   * 실패하면 기존 widget tree와 경계 정보를 그대로 유지한다.
   */
  UserTextInputLineStatus render(std::string_view committed,
                                 std::size_t cursor_codepoint_index,
                                 std::uint32_t composing_codepoint = 0U) noexcept;

  void set_active(bool active) noexcept;
  void reset_blink() noexcept;
  void set_selection_handler(SelectionHandler handler, void* context) noexcept;
  void set_colors(lv_color_t text, lv_color_t accent) noexcept;

  [[nodiscard]] lv_obj_t* object() const noexcept { return root_; }
  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] bool caret_visible() const noexcept { return caret_visible_; }
  [[nodiscard]] bool composition_visible() const noexcept {
    return composition_underline_ != nullptr;
  }
  [[nodiscard]] std::size_t display_codepoint_count() const noexcept {
    return display_codepoint_count_;
  }
  [[nodiscard]] lv_coord_t caret_x() const noexcept { return caret_x_; }
  [[nodiscard]] lv_coord_t scroll_x() const noexcept;
  [[nodiscard]] std::size_t nearest_cursor_index(lv_coord_t content_x) const noexcept;

 private:
  static void handle_delete(lv_event_t* event) noexcept;
  static void handle_pressed(lv_event_t* event) noexcept;
  static void handle_blink(lv_timer_t* timer) noexcept;

  void set_caret_visible(bool visible) noexcept;
  void ensure_caret_visible() noexcept;

  lv_obj_t* root_ = nullptr;
  lv_obj_t* content_ = nullptr;
  lv_obj_t* caret_ = nullptr;
  lv_obj_t* composition_underline_ = nullptr;
  lv_timer_t* blink_timer_ = nullptr;
  FontRepository* fonts_ = nullptr;
  SelectionHandler selection_handler_ = nullptr;
  void* selection_context_ = nullptr;
  std::array<lv_coord_t, kMaxDisplayCodepoints + 1U> boundaries_{};
  std::size_t display_codepoint_count_ = 0U;
  lv_coord_t width_ = 0;
  lv_coord_t height_ = 0;
  lv_coord_t caret_x_ = 0;
  lv_color_t text_color_ = lv_color_white();
  lv_color_t accent_color_ = lv_palette_main(LV_PALETTE_BLUE);
  bool active_ = false;
  bool caret_visible_ = false;
};

static_assert(sizeof(UserTextInputLine) <= 512U,
              "UserTextInputLine must keep only bounded glyph geometry");

}  // namespace hmi::ui
