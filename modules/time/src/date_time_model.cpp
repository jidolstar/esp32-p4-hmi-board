#include "hmi/time/date_time_model.hpp"

namespace hmi::time {

void DateTimeModel::begin(std::uint32_t now_ms) noexcept {
  value_ = {};
  source_ = TimeSource::unset;
  last_tick_ms_ = now_ms;
  remainder_ms_ = 0U;
  ++revision_;
}

void DateTimeModel::tick(std::uint32_t now_ms) noexcept {
  // unsigned 차이는 단조 millisecond counter가 한 번 wrap해도 경과 시간을 보존한다.
  const std::uint32_t elapsed_ms = now_ms - last_tick_ms_;
  last_tick_ms_ = now_ms;
  if (!has_value()) {
    return;
  }

  const std::uint64_t accumulated =
      static_cast<std::uint64_t>(remainder_ms_) + elapsed_ms;
  const std::uint32_t seconds =
      static_cast<std::uint32_t>(accumulated / 1000U);
  remainder_ms_ = static_cast<std::uint32_t>(accumulated % 1000U);
  if (seconds != 0U) {
    advance_seconds(value_, seconds);
    ++revision_;
  }
}

bool DateTimeModel::apply(const DateTimeValue& value, TimeSource source,
                          std::uint32_t now_ms) noexcept {
  if (!is_valid(value) || source == TimeSource::unset) {
    return false;
  }
  value_ = value;
  source_ = source;
  last_tick_ms_ = now_ms;
  remainder_ms_ = 0U;
  ++revision_;
  return true;
}

void DateTimeModel::clear(std::uint32_t now_ms) noexcept {
  value_ = {};
  source_ = TimeSource::unset;
  last_tick_ms_ = now_ms;
  remainder_ms_ = 0U;
  ++revision_;
}

}  // namespace hmi::time
