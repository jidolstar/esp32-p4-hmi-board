#include "hmi/time/date_time.hpp"

namespace hmi::time {

bool DateTimeValue::operator==(const DateTimeValue& other) const noexcept {
  return year == other.year && month == other.month && day == other.day &&
         hour == other.hour && minute == other.minute && second == other.second;
}

bool is_leap_year(std::uint16_t year) noexcept {
  return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

std::uint8_t days_in_month(std::uint16_t year,
                           std::uint8_t month) noexcept {
  static constexpr std::uint8_t kDays[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                            31U, 31U, 30U, 31U, 30U, 31U};
  if (month < 1U || month > 12U) {
    return 0U;
  }
  if (month == 2U && is_leap_year(year)) {
    return 29U;
  }
  return kDays[month - 1U];
}

bool is_valid(const DateTimeValue& value) noexcept {
  if (value.year < 2000U || value.year > 2099U) {
    return false;
  }
  const std::uint8_t maximum_day = days_in_month(value.year, value.month);
  return maximum_day != 0U && value.day >= 1U && value.day <= maximum_day &&
         value.hour <= 23U && value.minute <= 59U && value.second <= 59U;
}

std::uint8_t weekday_index(const DateTimeValue& value) noexcept {
  if (!is_valid(value)) {
    return 0U;
  }
  std::uint32_t days = 0U;
  for (std::uint16_t year = 2000U; year < value.year; ++year) {
    days += is_leap_year(year) ? 366U : 365U;
  }
  for (std::uint8_t month = 1U; month < value.month; ++month) {
    days += days_in_month(value.year, month);
  }
  days += static_cast<std::uint32_t>(value.day - 1U);
  return static_cast<std::uint8_t>((days + 6U) % 7U);
}

void advance_seconds(DateTimeValue& value, std::uint32_t seconds) noexcept {
  if (!is_valid(value) || seconds == 0U) {
    return;
  }

  std::uint32_t total = static_cast<std::uint32_t>(value.second) + seconds;
  value.second = static_cast<std::uint8_t>(total % 60U);
  total = static_cast<std::uint32_t>(value.minute) + total / 60U;
  value.minute = static_cast<std::uint8_t>(total % 60U);
  total = static_cast<std::uint32_t>(value.hour) + total / 60U;
  value.hour = static_cast<std::uint8_t>(total % 24U);
  std::uint32_t days = total / 24U;

  while (days > 0U && value.year <= 2099U) {
    const std::uint8_t remaining = static_cast<std::uint8_t>(
        days_in_month(value.year, value.month) - value.day);
    if (days <= remaining) {
      value.day = static_cast<std::uint8_t>(value.day + days);
      break;
    }
    days -= static_cast<std::uint32_t>(remaining) + 1U;
    value.day = 1U;
    ++value.month;
    if (value.month > 12U) {
      value.month = 1U;
      ++value.year;
    }
  }
}

}  // namespace hmi::time
