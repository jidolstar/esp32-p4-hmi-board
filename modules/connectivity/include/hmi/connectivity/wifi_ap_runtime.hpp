#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/wifi_ap_port.hpp"
#include "hmi/connectivity/wifi_sta_registration.hpp"

namespace hmi::connectivity {

enum class WifiApStationPolicy : std::uint8_t {
  suspend_and_restore,
  keep_connected,
};

enum class WifiApRuntimeState : std::uint8_t {
  off,
  preparing,
  starting,
  ready,
  stopping,
  restoring_sta,
  failed,
};

enum class WifiApRuntimeError : std::uint8_t {
  none,
  invalid_config,
  radio_busy,
  start_failed,
  start_timeout,
  stop_timeout,
  restore_failed,
};

/** 화면에 비밀번호를 복제하지 않는 AP 진단 상태다. */
struct WifiApRuntimeSnapshot {
  WifiApRuntimeState state = WifiApRuntimeState::off;
  WifiApRuntimeError error = WifiApRuntimeError::none;
  std::uint8_t client_count = 0U;
  bool health_server_ready = false;
  bool sta_suspended = false;
  bool sta_has_ip = false;
};

/**
 * AP 진단 흐름과 STA 공존 정책의 radio coordinator이자 단일 owner다.
 *
 * suspend 정책은 AP 전후로 STA를 정리·복구하고, keep 정책은 연결된 STA를 건드리지
 * 않는다. host는 radio마다 이 객체 하나만 구성한다. config 하나만 고정 크기로
 * 보관하며 snapshot에는 password를 넣지 않는다.
 */
class WifiApRuntime {
 public:
  WifiApRuntime(
      WifiStaRegistration& station, WifiApPort& port,
      WifiApStationPolicy station_policy =
          WifiApStationPolicy::suspend_and_restore) noexcept
      : station_(station), port_(port), station_policy_(station_policy) {}
  ~WifiApRuntime();

  WifiApRuntime(const WifiApRuntime&) = delete;
  WifiApRuntime& operator=(const WifiApRuntime&) = delete;

  bool start(const char* ssid, const char* password,
             std::uint32_t now_ms) noexcept;
  bool stop(std::uint32_t now_ms) noexcept;
  void tick(std::uint32_t now_ms) noexcept;
  void shutdown(std::uint32_t now_ms) noexcept;

  [[nodiscard]] WifiApRuntimeSnapshot snapshot() const noexcept {
    return snapshot_;
  }
  void copy_ssid(char* output, std::size_t capacity) const noexcept;
  void copy_password(char* output, std::size_t capacity) const noexcept;
  void copy_ipv4(char* output, std::size_t capacity) const noexcept;
  void copy_sta_ipv4(char* output, std::size_t capacity) const noexcept;

 private:
  void fail(WifiApRuntimeError error, std::uint32_t now_ms) noexcept;
  void begin_sta_restore(std::uint32_t now_ms) noexcept;
  void clear_config() noexcept;
  static bool copy_config_text(const char* source, char* output,
                               std::size_t capacity,
                               std::uint8_t& length) noexcept;

  WifiStaRegistration& station_;
  WifiApPort& port_;
  WifiApConfig config_{};
  WifiApRuntimeSnapshot snapshot_{};
  std::uint32_t state_started_ms_ = 0U;
  bool restore_started_ = false;
  WifiApStationPolicy station_policy_ =
      WifiApStationPolicy::suspend_and_restore;
};

static_assert(sizeof(WifiApRuntimeSnapshot) <= 8U,
              "AP runtime snapshot must stay small and secret-free");
static_assert(sizeof(WifiApRuntime) <= 160U,
              "AP runtime owns only one bounded credential");

}  // namespace hmi::connectivity
