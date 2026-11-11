#pragma once

#include <cstdint>

#include "hmi/time/date_time.hpp"

namespace hmi::time {

enum class TimeSource : std::uint8_t {
  unset,
  manual,
  rtc,
  network,
};

/**
 * 외부에서 보정된 wall time을 화면 표시용 software clock으로 진행한다.
 *
 * RTC·SNTP·저장소를 알지 않으며 composition root가 소유한다. 보정 사이의 초 진행은
 * `tick()`에 전달된 단조 시각만 사용하므로 wall clock 변경이 timeout에 영향을 주지 않는다.
 */
class DateTimeModel {
 public:
  void begin(std::uint32_t now_ms) noexcept;
  void tick(std::uint32_t now_ms) noexcept;
  [[nodiscard]] bool apply(const DateTimeValue& value, TimeSource source,
                           std::uint32_t now_ms) noexcept;
  void clear(std::uint32_t now_ms) noexcept;

  [[nodiscard]] const DateTimeValue& value() const noexcept { return value_; }
  [[nodiscard]] bool has_value() const noexcept {
    return source_ != TimeSource::unset;
  }
  [[nodiscard]] TimeSource source() const noexcept { return source_; }
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  DateTimeValue value_{};
  TimeSource source_ = TimeSource::unset;
  std::uint32_t last_tick_ms_ = 0U;
  std::uint32_t remainder_ms_ = 0U;
  std::uint32_t revision_ = 0U;
};

static_assert(sizeof(DateTimeModel) <= 24U,
              "DateTimeModel must remain a small scalar model");

}  // namespace hmi::time
