#pragma once

#include <chrono>
#include <cstdint>

#include "hmi/time/time_sync_controller.hpp"

namespace hmi::platform::windows::time {

/**
 * Windows 실행기에 가상 RTC·system clock·network time port를 제공한다.
 *
 * PC의 OS clock은 변경하지 않는다. 기본 RTC와 network 결과는 host 로컬 시각을 읽고,
 * 수동 설정 이후의 진행은 `steady_clock`으로 계산한다. fault flag는 결정적 자동 검사에서
 * 실패 경로를 주입할 때만 사용한다.
 */
class VirtualTimeSyncPorts {
 public:
  VirtualTimeSyncPorts();

  [[nodiscard]] hmi::time::TimeSyncPorts ports() noexcept;
  void set_has_ip(bool value) noexcept { has_ip_ = value; }
  void tick() noexcept;
  void set_network_time(const hmi::time::DateTimeValue& value) noexcept;
  void use_host_network_time() noexcept { network_override_ = false; }
  void set_rtc(const hmi::time::DateTimeValue& value,
               hmi::time::RtcStatus status =
                   hmi::time::RtcStatus::ready) noexcept;

  bool fail_rtc_write = false;
  bool fail_system_write = false;
  bool fail_system_read = false;
  bool fail_request = false;
  bool suppress_network_result = false;

  [[nodiscard]] unsigned request_count() const noexcept {
    return request_count_;
  }
  [[nodiscard]] unsigned rtc_write_count() const noexcept {
    return rtc_write_count_;
  }

 private:
  static hmi::time::RtcStatus read_rtc(void*,
                                        hmi::time::DateTimeValue&);
  static hmi::time::RtcStatus write_rtc(
      void*, const hmi::time::DateTimeValue&);
  static bool set_system(void*, const hmi::time::DateTimeValue&);
  static bool read_system(void*, hmi::time::DateTimeValue&);
  static bool request_sync(void*);
  static std::uint32_t sync_generation(void*);
  static bool has_ip(void*);
  static bool read_host_local(hmi::time::DateTimeValue& value) noexcept;
  static bool read_advancing(
      const hmi::time::DateTimeValue& base,
      const std::chrono::steady_clock::time_point& set_at,
      hmi::time::DateTimeValue& value) noexcept;

  hmi::time::DateTimeValue rtc_value_{};
  hmi::time::DateTimeValue system_value_{};
  hmi::time::DateTimeValue network_value_{};
  std::chrono::steady_clock::time_point rtc_set_at_{};
  std::chrono::steady_clock::time_point system_set_at_{};
  hmi::time::RtcStatus rtc_status_ = hmi::time::RtcStatus::invalid;
  bool system_valid_ = false;
  bool has_ip_ = false;
  bool request_pending_ = false;
  bool network_override_ = false;
  std::uint32_t generation_ = 0U;
  unsigned request_count_ = 0U;
  unsigned rtc_write_count_ = 0U;
};

static_assert(sizeof(VirtualTimeSyncPorts) <= 128U,
              "VirtualTimeSyncPorts must remain a small fixture");

}  // namespace hmi::platform::windows::time
