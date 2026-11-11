#include "time/virtual_time_sync_ports.hpp"

#include <ctime>

namespace hmi::platform::windows::time {

VirtualTimeSyncPorts::VirtualTimeSyncPorts() {
  hmi::time::DateTimeValue initial{};
  const auto now = std::chrono::steady_clock::now();
  if (read_host_local(initial)) {
    rtc_value_ = initial;
    system_value_ = initial;
    rtc_status_ = hmi::time::RtcStatus::ready;
    system_valid_ = true;
  }
  rtc_set_at_ = now;
  system_set_at_ = now;
}

hmi::time::TimeSyncPorts VirtualTimeSyncPorts::ports() noexcept {
  hmi::time::TimeSyncPorts result{};
  result.rtc = {this, read_rtc, write_rtc};
  result.system = {this, set_system, read_system};
  result.network = {this, request_sync, sync_generation};
  result.connectivity = {this, has_ip};
  return result;
}

void VirtualTimeSyncPorts::tick() noexcept {
  if (!request_pending_ || !has_ip_) {
    return;
  }
  request_pending_ = false;
  if (suppress_network_result) {
    return;
  }

  hmi::time::DateTimeValue synchronized{};
  if (network_override_) {
    synchronized = network_value_;
  } else if (!read_host_local(synchronized)) {
    return;
  }
  system_value_ = synchronized;
  system_set_at_ = std::chrono::steady_clock::now();
  system_valid_ = true;
  ++generation_;
}

void VirtualTimeSyncPorts::set_network_time(
    const hmi::time::DateTimeValue& value) noexcept {
  network_value_ = value;
  network_override_ = hmi::time::is_valid(value);
}

void VirtualTimeSyncPorts::set_rtc(const hmi::time::DateTimeValue& value,
                                   hmi::time::RtcStatus status) noexcept {
  rtc_value_ = value;
  rtc_status_ = status;
  rtc_set_at_ = std::chrono::steady_clock::now();
}

hmi::time::RtcStatus VirtualTimeSyncPorts::read_rtc(
    void* context, hmi::time::DateTimeValue& value) {
  auto& self = *static_cast<VirtualTimeSyncPorts*>(context);
  if (self.rtc_status_ != hmi::time::RtcStatus::ready) {
    return self.rtc_status_;
  }
  if (!read_advancing(self.rtc_value_, self.rtc_set_at_, value)) {
    return hmi::time::RtcStatus::invalid;
  }
  return hmi::time::RtcStatus::ready;
}

hmi::time::RtcStatus VirtualTimeSyncPorts::write_rtc(
    void* context, const hmi::time::DateTimeValue& value) {
  auto& self = *static_cast<VirtualTimeSyncPorts*>(context);
  ++self.rtc_write_count_;
  if (self.fail_rtc_write || !hmi::time::is_valid(value)) {
    return hmi::time::RtcStatus::write_error;
  }
  self.rtc_value_ = value;
  self.rtc_set_at_ = std::chrono::steady_clock::now();
  self.rtc_status_ = hmi::time::RtcStatus::ready;
  return hmi::time::RtcStatus::ready;
}

bool VirtualTimeSyncPorts::set_system(
    void* context, const hmi::time::DateTimeValue& value) {
  auto& self = *static_cast<VirtualTimeSyncPorts*>(context);
  if (self.fail_system_write || !hmi::time::is_valid(value)) {
    return false;
  }
  self.system_value_ = value;
  self.system_set_at_ = std::chrono::steady_clock::now();
  self.system_valid_ = true;
  return true;
}

bool VirtualTimeSyncPorts::read_system(
    void* context, hmi::time::DateTimeValue& value) {
  auto& self = *static_cast<VirtualTimeSyncPorts*>(context);
  return !self.fail_system_read && self.system_valid_ &&
         read_advancing(self.system_value_, self.system_set_at_, value);
}

bool VirtualTimeSyncPorts::request_sync(void* context) {
  auto& self = *static_cast<VirtualTimeSyncPorts*>(context);
  ++self.request_count_;
  if (self.fail_request || !self.has_ip_) {
    return false;
  }
  self.request_pending_ = true;
  return true;
}

std::uint32_t VirtualTimeSyncPorts::sync_generation(void* context) {
  return static_cast<VirtualTimeSyncPorts*>(context)->generation_;
}

bool VirtualTimeSyncPorts::has_ip(void* context) {
  return static_cast<VirtualTimeSyncPorts*>(context)->has_ip_;
}

bool VirtualTimeSyncPorts::read_host_local(
    hmi::time::DateTimeValue& value) noexcept {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  if (now == static_cast<std::time_t>(-1) || localtime_s(&local, &now) != 0) {
    return false;
  }
  const hmi::time::DateTimeValue converted{
      static_cast<std::uint16_t>(local.tm_year + 1900),
      static_cast<std::uint8_t>(local.tm_mon + 1),
      static_cast<std::uint8_t>(local.tm_mday),
      static_cast<std::uint8_t>(local.tm_hour),
      static_cast<std::uint8_t>(local.tm_min),
      static_cast<std::uint8_t>(local.tm_sec)};
  if (!hmi::time::is_valid(converted)) {
    return false;
  }
  value = converted;
  return true;
}

bool VirtualTimeSyncPorts::read_advancing(
    const hmi::time::DateTimeValue& base,
    const std::chrono::steady_clock::time_point& set_at,
    hmi::time::DateTimeValue& value) noexcept {
  if (!hmi::time::is_valid(base)) {
    return false;
  }
  value = base;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - set_at)
                           .count();
  if (seconds > 0) {
    hmi::time::advance_seconds(value,
                               static_cast<std::uint32_t>(seconds));
  }
  return hmi::time::is_valid(value);
}

}  // namespace hmi::platform::windows::time
