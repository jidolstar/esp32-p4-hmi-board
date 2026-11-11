#include <cassert>
#include <ctime>

#include "hmi/time/time_sync_controller.hpp"
#include "time/virtual_time_sync_ports.hpp"

int main() {
  using hmi::time::DateTimeModel;
  using hmi::time::DateTimeValue;
  using hmi::time::ManualApplyResult;
  using hmi::time::NetworkState;
  using hmi::time::TimeSource;
  using hmi::time::TimeSyncController;
  using hmi::time::TimeSyncPolicy;

  hmi::platform::windows::time::VirtualTimeSyncPorts virtual_time;
  virtual_time.set_rtc({2026U, 11U, 11U, 9U, 0U, 0U});
  virtual_time.set_network_time({2026U, 11U, 11U, 9U, 0U, 5U});

  DateTimeModel display;
  display.begin(0U);
  auto ports = virtual_time.ports();
  ports.update = hmi::time::date_time_model_sink(display);
  TimeSyncPolicy policy{};
  policy.sync_timeout_ms = 10U;
  policy.regular_sync_interval_ms = 10U;
  policy.first_retry_ms = 5U;
  policy.second_retry_ms = 10U;
  policy.third_retry_ms = 15U;
  policy.maximum_retry_ms = 20U;

  TimeSyncController controller;
  assert(controller.begin(ports, 0U, policy));
  assert(display.source() == TimeSource::rtc);
  virtual_time.set_has_ip(true);
  controller.tick(1U);
  assert(virtual_time.request_count() == 1U);
  virtual_time.tick();
  controller.tick(2U);
  assert(display.source() == TimeSource::network);
  assert(display.value() ==
         (DateTimeValue{2026U, 11U, 11U, 9U, 0U, 5U}));
  assert(virtual_time.rtc_write_count() == 1U);

  const std::time_t os_before = std::time(nullptr);
  virtual_time.fail_rtc_write = true;
  const DateTimeValue manual{2027U, 1U, 1U, 0U, 0U, 0U};
  assert(controller.apply_manual(manual, 3U) ==
         ManualApplyResult::applied_rtc_write_failed);
  const std::time_t os_after = std::time(nullptr);
  assert(os_after >= os_before && os_after - os_before <= 1);
  assert(display.source() == TimeSource::manual);
  assert(display.value() == manual);

  virtual_time.suppress_network_result = true;
  controller.tick(12U);
  virtual_time.tick();
  controller.tick(22U);
  assert(controller.snapshot(22U).network_state == NetworkState::retry_waiting);
  return 0;
}
