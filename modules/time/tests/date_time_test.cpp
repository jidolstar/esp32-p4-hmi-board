#include <cassert>
#include <cstdint>

#include "hmi/time/date_time_model.hpp"

int main() {
  using hmi::time::DateTimeModel;
  using hmi::time::DateTimeValue;
  using hmi::time::TimeSource;

  assert(hmi::time::is_leap_year(2000U));
  assert(hmi::time::is_leap_year(2028U));
  assert(!hmi::time::is_leap_year(2100U));
  assert(hmi::time::days_in_month(2028U, 2U) == 29U);
  assert(hmi::time::days_in_month(2027U, 2U) == 28U);
  assert(hmi::time::days_in_month(2027U, 13U) == 0U);
  assert(hmi::time::is_valid({2028U, 2U, 29U, 23U, 59U, 59U}));
  assert(!hmi::time::is_valid({2027U, 2U, 29U, 0U, 0U, 0U}));
  assert(!hmi::time::is_valid({1999U, 12U, 31U, 23U, 59U, 59U}));

  DateTimeValue rollover{2027U, 12U, 31U, 23U, 59U, 59U};
  hmi::time::advance_seconds(rollover, 1U);
  assert(rollover == (DateTimeValue{2028U, 1U, 1U, 0U, 0U, 0U}));

  DateTimeModel model;
  model.begin(UINT32_MAX - 500U);
  assert(!model.has_value());
  const auto initial_revision = model.revision();
  const DateTimeValue rtc{2028U, 2U, 29U, 23U, 59U, 58U};
  assert(model.apply(rtc, TimeSource::rtc, UINT32_MAX - 500U));
  model.tick(499U);
  assert(model.value() ==
         (DateTimeValue{2028U, 2U, 29U, 23U, 59U, 59U}));
  model.tick(1499U);
  assert(model.value() == (DateTimeValue{2028U, 3U, 1U, 0U, 0U, 0U}));
  assert(model.source() == TimeSource::rtc);

  const auto before_invalid = model.revision();
  assert(!model.apply({2027U, 2U, 29U, 0U, 0U, 0U},
                      TimeSource::manual, 2000U));
  assert(!model.apply(rtc, TimeSource::unset, 2000U));
  assert(model.revision() == before_invalid);

  model.clear(3000U);
  assert(!model.has_value());
  assert(model.revision() > initial_revision);
  return 0;
}
