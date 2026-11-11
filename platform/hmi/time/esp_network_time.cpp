#include "time/esp_network_time.hpp"

#include <Arduino.h>
#include <esp_sntp.h>

#include <cstdlib>
#include <ctime>
#include <sys/time.h>

namespace hmi::platform::hmi_host::time {

std::atomic<EspNetworkTime*> EspNetworkTime::active_{nullptr};

hmi::time::SystemTimePort EspNetworkTime::system_port() noexcept {
  return {this, set_local, read_local};
}

hmi::time::NetworkTimePort EspNetworkTime::network_port() noexcept {
  return {this, request_sync, sync_generation};
}

bool EspNetworkTime::set_local(void* context,
                               const hmi::time::DateTimeValue& value) {
  auto* self = static_cast<EspNetworkTime*>(context);
  if (self == nullptr || !hmi::time::is_valid(value) ||
      !self->ensure_timezone()) {
    return false;
  }
  std::tm local{};
  local.tm_year = static_cast<int>(value.year) - 1900;
  local.tm_mon = static_cast<int>(value.month) - 1;
  local.tm_mday = value.day;
  local.tm_hour = value.hour;
  local.tm_min = value.minute;
  local.tm_sec = value.second;
  local.tm_isdst = -1;
  const std::time_t epoch = std::mktime(&local);
  if (epoch == static_cast<std::time_t>(-1)) {
    return false;
  }
  timeval system_time{epoch, 0};
  return settimeofday(&system_time, nullptr) == 0;
}

bool EspNetworkTime::read_local(void* context,
                                hmi::time::DateTimeValue& value) {
  auto* self = static_cast<EspNetworkTime*>(context);
  if (self == nullptr || !self->ensure_timezone()) {
    return false;
  }
  const std::time_t epoch = std::time(nullptr);
  std::tm local{};
  if (epoch == static_cast<std::time_t>(-1) ||
      localtime_r(&epoch, &local) == nullptr) {
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

bool EspNetworkTime::request_sync(void* context) {
  auto* self = static_cast<EspNetworkTime*>(context);
  if (self == nullptr || self->config_.timezone == nullptr ||
      self->config_.primary_server == nullptr || !self->ensure_timezone()) {
    return false;
  }
  EspNetworkTime* expected = nullptr;
  if (!active_.compare_exchange_strong(expected, self) && expected != self) {
    return false;
  }
  if (!self->configured_) {
    // Framework callback에서는 I2C, UI, logging과 heap 작업을 하지 않는다.
    sntp_set_time_sync_notification_cb(on_time_sync);
    sntp_set_sync_interval(self->config_.sync_interval_ms);
    configTzTime(self->config_.timezone, self->config_.primary_server,
                 self->config_.secondary_server,
                 self->config_.tertiary_server);
    self->configured_ = true;
    return true;
  }
  return sntp_restart();
}

std::uint32_t EspNetworkTime::sync_generation(void* context) {
  const auto* self = static_cast<EspNetworkTime*>(context);
  return self == nullptr
             ? 0U
             : self->generation_.load(std::memory_order_acquire);
}

void EspNetworkTime::on_time_sync(timeval*) {
  if (auto* self = active_.load(std::memory_order_acquire)) {
    self->generation_.fetch_add(1U, std::memory_order_release);
  }
}

bool EspNetworkTime::ensure_timezone() noexcept {
  if (config_.timezone == nullptr || setenv("TZ", config_.timezone, 1) != 0) {
    return false;
  }
  tzset();
  return true;
}

}  // namespace hmi::platform::hmi_host::time
