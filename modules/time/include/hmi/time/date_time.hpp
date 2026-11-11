#pragma once

#include <cstdint>

namespace hmi::time {

/**
 * RTC, system clock과 UI가 교환하는 framework 독립 로컬 날짜·시간 값이다.
 *
 * 하드웨어 register나 `std::tm`을 노출하지 않으며 2000~2099 범위의 달력 계산에 필요한
 * 작은 scalar만 소유한다.
 */
struct DateTimeValue {
  std::uint16_t year = 0U;
  std::uint8_t month = 0U;
  std::uint8_t day = 0U;
  std::uint8_t hour = 0U;
  std::uint8_t minute = 0U;
  std::uint8_t second = 0U;

  [[nodiscard]] bool operator==(const DateTimeValue& other) const noexcept;
  [[nodiscard]] bool operator!=(const DateTimeValue& other) const noexcept {
    return !(*this == other);
  }
};

[[nodiscard]] bool is_leap_year(std::uint16_t year) noexcept;
[[nodiscard]] std::uint8_t days_in_month(std::uint16_t year,
                                         std::uint8_t month) noexcept;
[[nodiscard]] bool is_valid(const DateTimeValue& value) noexcept;
[[nodiscard]] std::uint8_t weekday_index(const DateTimeValue& value) noexcept;
void advance_seconds(DateTimeValue& value, std::uint32_t seconds) noexcept;

static_assert(sizeof(DateTimeValue) <= 8U,
              "DateTimeValue must remain a small scalar value");

}  // namespace hmi::time
