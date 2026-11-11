#pragma once

#include <atomic>
#include <cstdint>

#include "hmi/time/time_sync_controller.hpp"

struct timeval;

namespace hmi::platform::hmi_host::time {

struct NetworkTimeConfig {
  const char* timezone = "KST-9";
  const char* primary_server = "time.cloudflare.com";
  const char* secondary_server = "pool.ntp.org";
  const char* tertiary_server = nullptr;
  std::uint32_t sync_interval_ms = 24U * 60U * 60U * 1000U;
};

/**
 * ESP32 system clock과 SNTP를 공통 time port로 노출한다.
 *
 * SNTP callback은 generation만 원자적으로 증가시킨다. timezone 변환, system clock 설정과
 * SNTP 시작·재시작은 app thread에서만 수행하며 firmware 안에서 한 인스턴스만 활성화한다.
 */
class EspNetworkTime {
 public:
  explicit EspNetworkTime(NetworkTimeConfig config = {}) noexcept
      : config_(config) {}

  [[nodiscard]] hmi::time::SystemTimePort system_port() noexcept;
  [[nodiscard]] hmi::time::NetworkTimePort network_port() noexcept;

 private:
  static bool set_local(void*, const hmi::time::DateTimeValue&);
  static bool read_local(void*, hmi::time::DateTimeValue&);
  static bool request_sync(void*);
  static std::uint32_t sync_generation(void*);
  static void on_time_sync(timeval* value);
  bool ensure_timezone() noexcept;

  NetworkTimeConfig config_{};
  std::atomic<std::uint32_t> generation_{0U};
  bool configured_ = false;
  static std::atomic<EspNetworkTime*> active_;
};

static_assert(sizeof(EspNetworkTime) <= 64U,
              "EspNetworkTime must remain a small adapter");

}  // namespace hmi::platform::hmi_host::time
