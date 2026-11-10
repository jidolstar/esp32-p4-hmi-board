#include "hmi/connectivity/wifi_ap_runtime.hpp"

#include <cstring>

namespace hmi::connectivity {
namespace {

constexpr std::uint32_t kTransitionTimeoutMs = 5000U;
constexpr std::uint32_t kRestoreTimeoutMs = 25000U;

void copy_text(const char* source, char* output,
               const std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0U) {
    return;
  }
  std::strncpy(output, source == nullptr ? "" : source, capacity - 1U);
  output[capacity - 1U] = '\0';
}

}  // namespace

WifiApRuntime::~WifiApRuntime() {
  // host가 먼저 shutdown하는 것이 정상 경로다. 소멸 중에는 transport만 즉시 정리한다.
  port_.request_stop();
  clear_config();
}

bool WifiApRuntime::copy_config_text(const char* source, char* output,
                                     const std::size_t capacity,
                                     std::uint8_t& length) noexcept {
  if (source == nullptr || output == nullptr || capacity < 2U) {
    return false;
  }
  const std::size_t size = strnlen(source, capacity);
  if (size == 0U || size >= capacity) {
    return false;
  }
  std::memcpy(output, source, size);
  output[size] = '\0';
  length = static_cast<std::uint8_t>(size);
  return true;
}

bool WifiApRuntime::start(const char* ssid, const char* password,
                          const std::uint32_t now_ms) noexcept {
  if (snapshot_.state != WifiApRuntimeState::off) {
    return false;
  }
  WifiApConfig candidate{};
  if (!copy_config_text(ssid, candidate.ssid, sizeof(candidate.ssid),
                        candidate.ssid_length) ||
      !copy_config_text(password, candidate.password,
                        sizeof(candidate.password),
                        candidate.password_length) ||
      candidate.password_length < 8U) {
    snapshot_.error = WifiApRuntimeError::invalid_config;
    return false;
  }
  config_ = candidate;
  std::memset(candidate.password, 0, sizeof(candidate.password));
  snapshot_ = {};
  snapshot_.state = WifiApRuntimeState::preparing;
  state_started_ms_ = now_ms;
  restore_started_ = false;
  if (station_policy_ == WifiApStationPolicy::suspend_and_restore) {
    station_.cancel();
  }
  return true;
}

bool WifiApRuntime::stop(const std::uint32_t now_ms) noexcept {
  if (snapshot_.state == WifiApRuntimeState::off ||
      snapshot_.state == WifiApRuntimeState::stopping ||
      snapshot_.state == WifiApRuntimeState::restoring_sta) {
    return false;
  }
  port_.request_stop();
  snapshot_.state = WifiApRuntimeState::stopping;
  snapshot_.health_server_ready = false;
  snapshot_.client_count = 0U;
  state_started_ms_ = now_ms;
  return true;
}

void WifiApRuntime::fail(const WifiApRuntimeError error,
                         const std::uint32_t now_ms) noexcept {
  port_.request_stop();
  snapshot_.state = WifiApRuntimeState::failed;
  snapshot_.error = error;
  snapshot_.health_server_ready = false;
  snapshot_.client_count = 0U;
  state_started_ms_ = now_ms;
  if (station_.suspended_for_ap()) {
    station_.resume_after_ap(now_ms);
  }
  snapshot_.sta_suspended = station_.suspended_for_ap();
}

void WifiApRuntime::begin_sta_restore(const std::uint32_t now_ms) noexcept {
  snapshot_.state = WifiApRuntimeState::restoring_sta;
  state_started_ms_ = now_ms;
  restore_started_ = station_.resume_after_ap(now_ms);
  snapshot_.sta_suspended = station_.suspended_for_ap();
  if (!restore_started_ && snapshot_.sta_suspended) {
    fail(WifiApRuntimeError::restore_failed, now_ms);
  }
}

void WifiApRuntime::tick(const std::uint32_t now_ms) noexcept {
  port_.poll();
  const WifiApPortSnapshot port = port_.snapshot();
  const WifiRegistrationSnapshot station = station_.snapshot();
  snapshot_.client_count = port.client_count;
  snapshot_.health_server_ready = port.health_server_ready;
  snapshot_.sta_suspended = station_.suspended_for_ap();
  snapshot_.sta_has_ip = station.has_ip;

  switch (snapshot_.state) {
    case WifiApRuntimeState::off:
      return;
    case WifiApRuntimeState::preparing:
      if (station_policy_ == WifiApStationPolicy::keep_connected) {
        if (station_.busy()) {
          if (now_ms - state_started_ms_ >= kTransitionTimeoutMs) {
            fail(WifiApRuntimeError::radio_busy, now_ms);
          }
          return;
        }
        if (!port_.start(config_)) {
          fail(WifiApRuntimeError::start_failed, now_ms);
          return;
        }
        snapshot_.state = WifiApRuntimeState::starting;
        state_started_ms_ = now_ms;
        return;
      }
      station_.cancel();
      if (station_.busy()) {
        if (now_ms - state_started_ms_ >= kTransitionTimeoutMs) {
          fail(WifiApRuntimeError::radio_busy, now_ms);
        }
        return;
      }
      if (!station_.suspend_for_ap()) {
        fail(WifiApRuntimeError::radio_busy, now_ms);
        return;
      }
      snapshot_.sta_suspended = true;
      if (!port_.start(config_)) {
        fail(WifiApRuntimeError::start_failed, now_ms);
        return;
      }
      snapshot_.state = WifiApRuntimeState::starting;
      state_started_ms_ = now_ms;
      return;
    case WifiApRuntimeState::starting:
      if (port.state == WifiApPortState::ready &&
          port.health_server_ready) {
        snapshot_.state = WifiApRuntimeState::ready;
        snapshot_.error = WifiApRuntimeError::none;
      } else if (port.state == WifiApPortState::failed) {
        fail(WifiApRuntimeError::start_failed, now_ms);
      } else if (now_ms - state_started_ms_ >= kTransitionTimeoutMs) {
        fail(WifiApRuntimeError::start_timeout, now_ms);
      }
      return;
    case WifiApRuntimeState::ready:
      if (port.state == WifiApPortState::failed ||
          !port.health_server_ready) {
        fail(WifiApRuntimeError::start_failed, now_ms);
      }
      return;
    case WifiApRuntimeState::stopping:
      if (port.state == WifiApPortState::off) {
        if (station_.suspended_for_ap()) {
          begin_sta_restore(now_ms);
        } else {
          snapshot_ = {};
          snapshot_.sta_has_ip = station_.snapshot().has_ip;
          clear_config();
        }
      } else if (now_ms - state_started_ms_ >= kTransitionTimeoutMs) {
        fail(WifiApRuntimeError::stop_timeout, now_ms);
      }
      return;
    case WifiApRuntimeState::restoring_sta:
      if (station_.busy()) {
        if (now_ms - state_started_ms_ >= kRestoreTimeoutMs) {
          snapshot_.state = WifiApRuntimeState::failed;
          snapshot_.error = WifiApRuntimeError::restore_failed;
        }
        return;
      }
      if (restore_started_ &&
          station_.snapshot().state == WifiRegistrationState::failed) {
        snapshot_.state = WifiApRuntimeState::failed;
        snapshot_.error = WifiApRuntimeError::restore_failed;
        return;
      }
      snapshot_ = {};
      clear_config();
      return;
    case WifiApRuntimeState::failed:
      return;
  }
}

void WifiApRuntime::shutdown(const std::uint32_t now_ms) noexcept {
  port_.request_stop();
  if (station_.suspended_for_ap()) {
    station_.resume_after_ap(now_ms);
  }
  snapshot_ = {};
  clear_config();
}

void WifiApRuntime::copy_ssid(char* output,
                              const std::size_t capacity) const noexcept {
  copy_text(config_.ssid, output, capacity);
}

void WifiApRuntime::copy_password(char* output,
                                  const std::size_t capacity) const noexcept {
  copy_text(config_.password, output, capacity);
}

void WifiApRuntime::copy_ipv4(char* output,
                              const std::size_t capacity) const noexcept {
  port_.copy_ipv4(output, capacity);
}

void WifiApRuntime::copy_sta_ipv4(char* output,
                                  const std::size_t capacity) const noexcept {
  copy_text(station_.snapshot().ipv4, output, capacity);
}

void WifiApRuntime::clear_config() noexcept {
  // 비밀번호 정리가 최적화로 제거되지 않도록 volatile byte overwrite를 사용한다.
  volatile auto* bytes = reinterpret_cast<volatile std::uint8_t*>(&config_);
  for (std::size_t index = 0U; index < sizeof(config_); ++index) {
    bytes[index] = 0U;
  }
}

}  // namespace hmi::connectivity
