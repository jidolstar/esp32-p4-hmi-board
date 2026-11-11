#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "hmi/time/time_sync_controller.hpp"
#include "generated/text_catalog.hpp"
#include "page_contract.hpp"

namespace hmi::samples::suite {

enum class DateTimeField : std::uint8_t {
  year,
  month,
  day,
  hour,
  minute,
  second,
};

struct DateTimePageSnapshot {
  hmi::time::DateTimeValue current{};
  hmi::time::DateTimeValue draft{};
  hmi::time::TimeSyncSnapshot sync{};
  hmi::time::ManualApplyResult last_apply =
      hmi::time::ManualApplyResult::invalid_value;
  hmi::time::TimeSource source = hmi::time::TimeSource::unset;
  bool has_current = false;
  bool has_apply_result = false;
  bool draft_modified = false;
};

/**
 * 현재 시각과 동기화 진단을 표시하고 수동 날짜·시각 초안을 적용하는 page다.
 *
 * 시간 model과 controller는 composition root가 소유한다. page는 두 객체를 빌려 읽고
 * 사용자 초안만 소유하며 RTC, SNTP 또는 system clock API를 직접 호출하지 않는다.
 */
class DateTimePage final : public SamplePage {
 public:
  DateTimePage() = default;
  DateTimePage(const DateTimePage&) = delete;
  DateTimePage& operator=(const DateTimePage&) = delete;
  ~DateTimePage() override;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override { return {}; }

  bool adjust(DateTimeField field, int delta) noexcept;
  bool set_draft(const hmi::time::DateTimeValue& value) noexcept;
  void advance_draft_seconds(std::uint32_t seconds) noexcept;
  hmi::time::ManualApplyResult apply_draft() noexcept;
  void reset_draft() noexcept;
  [[nodiscard]] DateTimePageSnapshot date_time_snapshot() const noexcept;

 private:
  static void handle_event(lv_event_t* event);
  static void handle_timer(lv_timer_t* timer) noexcept;

  bool create_header(lv_obj_t* parent) noexcept;
  bool create_body(lv_obj_t* parent) noexcept;
  bool create_footer(lv_obj_t* parent) noexcept;
  lv_obj_t* create_label(lv_obj_t* parent, generated::TextId id,
                         hmi::ui::TextRole role) noexcept;
  lv_obj_t* create_button(lv_obj_t* parent, generated::TextId id,
                          hmi::ui::ButtonVariant variant) noexcept;
  bool create_field(lv_obj_t* parent, std::size_t index,
                    generated::TextId title) noexcept;
  void refresh() noexcept;
  void refresh_current() noexcept;
  void refresh_draft() noexcept;
  void refresh_status() noexcept;
  void clamp_day() noexcept;

  PageContext context_{};
  DateTimePageSnapshot state_{};
  lv_obj_t* root_ = nullptr;
  lv_obj_t* back_button_ = nullptr;
  lv_obj_t* theme_button_ = nullptr;
  lv_obj_t* locale_ko_button_ = nullptr;
  lv_obj_t* locale_en_button_ = nullptr;
  lv_obj_t* current_date_label_ = nullptr;
  lv_obj_t* current_time_label_ = nullptr;
  lv_obj_t* source_value_label_ = nullptr;
  lv_obj_t* rtc_value_label_ = nullptr;
  lv_obj_t* network_value_label_ = nullptr;
  std::array<lv_obj_t*, 6U> draft_value_labels_{};
  std::array<lv_obj_t*, 6U> decrement_buttons_{};
  std::array<lv_obj_t*, 6U> increment_buttons_{};
  lv_obj_t* reset_button_ = nullptr;
  lv_obj_t* apply_button_ = nullptr;
  lv_obj_t* result_label_ = nullptr;
  lv_timer_t* timer_ = nullptr;
  std::uint32_t editor_tick_ms_ = 0U;
  std::uint32_t editor_remainder_ms_ = 0U;
};

static_assert(sizeof(DateTimePage) <= 512U,
              "DateTimePage must keep only bounded draft state and handles");

}  // namespace hmi::samples::suite
