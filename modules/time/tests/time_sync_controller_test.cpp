#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "hmi/time/time_sync_controller.hpp"

namespace {

using hmi::time::ConnectivityPort;
using hmi::time::DateTimeModel;
using hmi::time::DateTimeValue;
using hmi::time::ManualApplyResult;
using hmi::time::NetworkState;
using hmi::time::NetworkTimePort;
using hmi::time::RtcStatus;
using hmi::time::RtcTimePort;
using hmi::time::SystemTimePort;
using hmi::time::TimeSource;
using hmi::time::TimeSyncController;
using hmi::time::TimeSyncError;
using hmi::time::TimeSyncPolicy;
using hmi::time::TimeSyncPorts;

struct FakeEnvironment {
  DateTimeValue rtc_value{2026U, 11U, 11U, 9U, 0U, 0U};
  DateTimeValue system_value{2026U, 11U, 11U, 9U, 0U, 0U};
  DateTimeValue last_rtc_write{};
  RtcStatus rtc_read_status = RtcStatus::ready;
  RtcStatus rtc_write_status = RtcStatus::ready;
  bool system_set_ok = true;
  bool system_read_ok = true;
  bool has_ip = false;
  bool request_ok = true;
  std::uint32_t generation = 0U;
  unsigned rtc_reads = 0U;
  unsigned rtc_writes = 0U;
  unsigned system_sets = 0U;
  unsigned requests = 0U;
};

RtcStatus read_rtc(void* context, DateTimeValue& value) {
  auto& fake = *static_cast<FakeEnvironment*>(context);
  ++fake.rtc_reads;
  if (fake.rtc_read_status == RtcStatus::ready) {
    value = fake.rtc_value;
  }
  return fake.rtc_read_status;
}

RtcStatus write_rtc(void* context, const DateTimeValue& value) {
  auto& fake = *static_cast<FakeEnvironment*>(context);
  ++fake.rtc_writes;
  fake.last_rtc_write = value;
  if (fake.rtc_write_status == RtcStatus::ready) {
    fake.rtc_value = value;
  }
  return fake.rtc_write_status;
}

bool set_system(void* context, const DateTimeValue& value) {
  auto& fake = *static_cast<FakeEnvironment*>(context);
  ++fake.system_sets;
  if (fake.system_set_ok) {
    fake.system_value = value;
  }
  return fake.system_set_ok;
}

bool read_system(void* context, DateTimeValue& value) {
  auto& fake = *static_cast<FakeEnvironment*>(context);
  if (fake.system_read_ok) {
    value = fake.system_value;
  }
  return fake.system_read_ok;
}

bool request_sync(void* context) {
  auto& fake = *static_cast<FakeEnvironment*>(context);
  ++fake.requests;
  return fake.request_ok;
}

std::uint32_t generation(void* context) {
  return static_cast<FakeEnvironment*>(context)->generation;
}

bool has_ip(void* context) {
  return static_cast<FakeEnvironment*>(context)->has_ip;
}

TimeSyncPorts ports(FakeEnvironment& fake, DateTimeModel& model,
                    bool with_rtc = true) {
  TimeSyncPorts result{};
  if (with_rtc) {
    result.rtc = RtcTimePort{&fake, read_rtc, write_rtc};
  }
  result.system = SystemTimePort{&fake, set_system, read_system};
  result.network = NetworkTimePort{&fake, request_sync, generation};
  result.connectivity = ConnectivityPort{&fake, has_ip};
  result.update = hmi::time::date_time_model_sink(model);
  return result;
}

TimeSyncPolicy fast_policy() {
  TimeSyncPolicy policy{};
  policy.sync_timeout_ms = 10U;
  policy.regular_sync_interval_ms = 100U;
  policy.first_retry_ms = 20U;
  policy.second_retry_ms = 40U;
  policy.third_retry_ms = 60U;
  policy.maximum_retry_ms = 80U;
  policy.rtc_write_threshold_seconds = 2U;
  return policy;
}

void boot_and_network_priority() {
  FakeEnvironment fake;
  fake.has_ip = true;
  DateTimeModel model;
  model.begin(100U);
  TimeSyncController controller;
  assert(controller.begin(ports(fake, model), 100U, fast_policy()));
  assert(model.source() == TimeSource::rtc);
  assert(controller.snapshot(100U).network_state == NetworkState::syncing);

  fake.system_value = {2026U, 11U, 11U, 9U, 0U, 5U};
  ++fake.generation;
  controller.tick(105U);
  const auto state = controller.snapshot(105U);
  assert(state.source == TimeSource::network);
  assert(state.network_synchronized && state.has_last_success);
  assert(state.has_last_rtc_drift && state.last_rtc_drift_seconds == 5U);
  assert(fake.rtc_writes == 1U && model.source() == TimeSource::network);
}

void drift_threshold_and_rtc_failure() {
  FakeEnvironment fake;
  fake.has_ip = true;
  DateTimeModel model;
  model.begin(0U);
  TimeSyncController controller;
  assert(controller.begin(ports(fake, model), 0U, fast_policy()));

  fake.system_value = {2026U, 11U, 11U, 9U, 0U, 2U};
  ++fake.generation;
  controller.tick(1U);
  assert(fake.rtc_writes == 0U);

  controller.tick(101U);
  fake.system_value = {2026U, 11U, 11U, 9U, 0U, 3U};
  fake.rtc_write_status = RtcStatus::write_error;
  ++fake.generation;
  controller.tick(102U);
  const auto state = controller.snapshot(102U);
  assert(state.source == TimeSource::network);
  assert(state.rtc_status == RtcStatus::write_error);
  assert(state.last_error == TimeSyncError::rtc_write);
}

void manual_results_and_network_replacement() {
  FakeEnvironment fake;
  DateTimeModel model;
  model.begin(0U);
  TimeSyncController controller;
  assert(controller.begin(ports(fake, model), 0U, fast_policy()));
  assert(controller.apply_manual({2026U, 2U, 30U, 0U, 0U, 0U}, 1U) ==
         ManualApplyResult::invalid_value);

  fake.system_set_ok = false;
  assert(controller.apply_manual({2026U, 12U, 31U, 23U, 59U, 59U}, 2U) ==
         ManualApplyResult::system_write_failed);
  fake.system_set_ok = true;
  fake.rtc_write_status = RtcStatus::write_error;
  assert(controller.apply_manual({2027U, 1U, 1U, 0U, 0U, 0U}, 3U) ==
         ManualApplyResult::applied_rtc_write_failed);
  assert(model.source() == TimeSource::manual);

  fake.has_ip = true;
  controller.tick(4U);
  fake.system_value = {2027U, 1U, 1U, 9U, 0U, 0U};
  ++fake.generation;
  controller.tick(5U);
  assert(model.source() == TimeSource::network);
}

void timeout_backoff_late_success() {
  FakeEnvironment fake;
  fake.has_ip = true;
  DateTimeModel model;
  model.begin(100U);
  TimeSyncController controller;
  assert(controller.begin(ports(fake, model), 100U, fast_policy()));
  controller.tick(110U);
  assert(controller.snapshot(110U).next_attempt_in_ms == 20U);
  controller.tick(130U);
  assert(fake.requests == 2U);
  controller.tick(140U);
  assert(controller.snapshot(140U).next_attempt_in_ms == 40U);

  fake.system_value = {2026U, 11U, 11U, 10U, 0U, 0U};
  ++fake.generation;
  controller.tick(150U);
  assert(controller.snapshot(150U).network_state == NetworkState::synced);
  assert(controller.snapshot(150U).consecutive_failures == 0U);
}

void reconnect_daily_and_wraparound() {
  FakeEnvironment fake;
  DateTimeModel model;
  const std::uint32_t start = UINT32_MAX - 5U;
  model.begin(start);
  TimeSyncController controller;
  assert(controller.begin(ports(fake, model), start, fast_policy()));

  fake.has_ip = true;
  controller.tick(2U);
  assert(fake.requests == 1U);
  fake.system_value = {2026U, 11U, 11U, 11U, 0U, 0U};
  ++fake.generation;
  controller.tick(3U);
  fake.has_ip = false;
  controller.tick(103U);
  assert(controller.snapshot(103U).sync_due);
  fake.has_ip = true;
  controller.tick(104U);
  assert(fake.requests == 2U);
}

void invalid_rtc_and_optional_rtc() {
  for (const auto status : {RtcStatus::invalid, RtcStatus::not_detected,
                            RtcStatus::read_error}) {
    FakeEnvironment fake;
    fake.rtc_read_status = status;
    fake.has_ip = true;
    DateTimeModel model;
    model.begin(0U);
    TimeSyncController controller;
    assert(controller.begin(ports(fake, model), 0U, fast_policy()));
    fake.system_value = {2026U, 11U, 11U, 13U, 0U, 0U};
    ++fake.generation;
    controller.tick(1U);
    assert(controller.snapshot(1U).source == TimeSource::network);
  }

  FakeEnvironment fake;
  fake.has_ip = true;
  DateTimeModel model;
  model.begin(0U);
  auto policy = fast_policy();
  policy.update_rtc_from_network = false;
  TimeSyncController controller;
  assert(controller.begin(ports(fake, model, false), 0U, policy));
  fake.system_value = {2026U, 11U, 11U, 14U, 0U, 0U};
  ++fake.generation;
  controller.tick(1U);
  assert(controller.snapshot(1U).last_error == TimeSyncError::none);
}

}  // namespace

int main() {
  boot_and_network_priority();
  drift_threshold_and_rtc_failure();
  manual_results_and_network_replacement();
  timeout_backoff_late_success();
  reconnect_daily_and_wraparound();
  invalid_rtc_and_optional_rtc();

  hmi::time::TimeSyncController invalid;
  assert(!invalid.begin({}, 0U, fast_policy()));
  assert(invalid.snapshot(0U).last_error == TimeSyncError::invalid_ports);
  return 0;
}
