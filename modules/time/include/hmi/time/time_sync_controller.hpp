#pragma once

#include <cstdint>

#include "hmi/time/date_time_model.hpp"

namespace hmi::time {

enum class RtcStatus : std::uint8_t {
  ready,
  invalid,
  not_detected,
  read_error,
  write_error,
};

enum class NetworkState : std::uint8_t {
  idle,
  waiting_for_ip,
  syncing,
  synced,
  retry_waiting,
};

enum class TimeSyncError : std::uint8_t {
  none,
  invalid_ports,
  rtc_read,
  system_write,
  network_start,
  network_timeout,
  system_read,
  rtc_write,
  invalid_manual_value,
};

enum class ManualApplyResult : std::uint8_t {
  applied,
  applied_rtc_write_failed,
  invalid_value,
  system_write_failed,
};

/** RTC 구현이 제공하는 로컬 wall time 입출력 계약이다. */
struct RtcTimePort {
  void* context = nullptr;
  RtcStatus (*read)(void*, DateTimeValue&) = nullptr;
  RtcStatus (*write)(void*, const DateTimeValue&) = nullptr;
};

/** 플랫폼 system clock과 로컬 날짜·시간 사이를 변환하는 계약이다. */
struct SystemTimePort {
  void* context = nullptr;
  bool (*set_local)(void*, const DateTimeValue&) = nullptr;
  bool (*read_local)(void*, DateTimeValue&) = nullptr;
};

/** SNTP 요청과 callback generation만 공통 core에 노출하는 계약이다. */
struct NetworkTimePort {
  void* context = nullptr;
  bool (*request_sync)(void*) = nullptr;
  std::uint32_t (*sync_generation)(void*) = nullptr;
};

/** Wi-Fi 구현과 무관하게 현재 IP 사용 가능 여부만 제공한다. */
struct ConnectivityPort {
  void* context = nullptr;
  bool (*has_ip)(void*) = nullptr;
};

/** 새 wall time을 화면 모델 또는 진단 소비자에 전달한다. */
struct TimeUpdateSink {
  void* context = nullptr;
  void (*apply)(void*, const DateTimeValue&, TimeSource,
                std::uint32_t now_ms) = nullptr;
};

struct TimeSyncPorts {
  RtcTimePort rtc{};
  SystemTimePort system{};
  NetworkTimePort network{};
  ConnectivityPort connectivity{};
  TimeUpdateSink update{};
};

/** 제품별 조립에서 주입할 수 있는 단조 시간 기반 동기화 정책이다. */
struct TimeSyncPolicy {
  std::uint32_t sync_timeout_ms = 15U * 1000U;
  std::uint32_t regular_sync_interval_ms = 24U * 60U * 60U * 1000U;
  std::uint32_t first_retry_ms = 30U * 1000U;
  std::uint32_t second_retry_ms = 2U * 60U * 1000U;
  std::uint32_t third_retry_ms = 10U * 60U * 1000U;
  std::uint32_t maximum_retry_ms = 60U * 60U * 1000U;
  std::uint32_t rtc_write_threshold_seconds = 2U;
  bool update_rtc_from_network = true;
};

struct TimeSyncSnapshot {
  TimeSource source = TimeSource::unset;
  RtcStatus rtc_status = RtcStatus::not_detected;
  NetworkState network_state = NetworkState::idle;
  TimeSyncError last_error = TimeSyncError::none;
  bool network_synchronized = false;
  bool sync_due = false;
  bool has_last_success = false;
  std::uint8_t consecutive_failures = 0U;
  std::uint32_t last_success_elapsed_ms = 0U;
  std::uint32_t next_attempt_in_ms = 0U;
  bool has_last_rtc_drift = false;
  std::uint32_t last_rtc_drift_seconds = 0U;
  std::uint32_t revision = 0U;
};

/**
 * RTC·수동 설정·network time의 우선순위와 재동기화 일정을 조정한다.
 *
 * port context와 callback은 controller보다 오래 살아야 한다. callback 경계에서는 sync
 * generation만 바뀌며 RTC와 UI 갱신은 `tick()` 호출자가 수행한다. task와 heap을 소유하지
 * 않고 작은 scalar 상태만 유지한다.
 */
class TimeSyncController {
 public:
  [[nodiscard]] bool begin(const TimeSyncPorts& ports, std::uint32_t now_ms,
                           const TimeSyncPolicy& policy = {}) noexcept;
  void tick(std::uint32_t now_ms) noexcept;
  [[nodiscard]] ManualApplyResult apply_manual(
      const DateTimeValue& value) noexcept;
  [[nodiscard]] ManualApplyResult apply_manual(
      const DateTimeValue& value, std::uint32_t now_ms) noexcept;
  [[nodiscard]] TimeSyncSnapshot snapshot() const noexcept;
  [[nodiscard]] TimeSyncSnapshot snapshot(
      std::uint32_t now_ms) const noexcept;

 private:
  void publish(const DateTimeValue& value, TimeSource source,
               std::uint32_t now_ms) noexcept;
  void request_network_sync(std::uint32_t now_ms) noexcept;
  void mark_network_failure(std::uint32_t now_ms,
                            TimeSyncError error) noexcept;
  [[nodiscard]] bool consume_network_sync(std::uint32_t now_ms) noexcept;
  void adjust_rtc_from_network(const DateTimeValue& value) noexcept;
  [[nodiscard]] std::uint32_t retry_delay() const noexcept;

  TimeSyncPorts ports_{};
  TimeSyncPolicy policy_{};
  TimeSource source_ = TimeSource::unset;
  RtcStatus rtc_status_ = RtcStatus::not_detected;
  NetworkState network_state_ = NetworkState::idle;
  TimeSyncError last_error_ = TimeSyncError::none;
  bool initialized_ = false;
  bool had_ip_ = false;
  bool network_synchronized_ = false;
  bool sync_due_ = false;
  bool has_last_success_ = false;
  bool retry_scheduled_ = false;
  bool has_last_rtc_drift_ = false;
  std::uint8_t consecutive_failures_ = 0U;
  std::uint32_t observed_generation_ = 0U;
  std::uint32_t current_ms_ = 0U;
  std::uint32_t attempt_started_ms_ = 0U;
  std::uint32_t retry_started_ms_ = 0U;
  std::uint32_t last_success_ms_ = 0U;
  std::uint32_t last_rtc_drift_seconds_ = 0U;
  std::uint32_t revision_ = 0U;
};

static_assert(sizeof(TimeSyncSnapshot) <= 40U,
              "TimeSyncSnapshot must remain a bounded scalar snapshot");
static_assert(sizeof(TimeSyncController) <= 256U,
              "TimeSyncController must remain a small scalar model");

/** composition root가 소유한 표시 model과 동기화 controller의 비소유 묶음이다. */
struct TimeRuntime {
  DateTimeModel* model = nullptr;
  TimeSyncController* controller = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return model != nullptr && controller != nullptr;
  }
};

static_assert(sizeof(TimeRuntime) <= 16U,
              "TimeRuntime must contain only two non-owning pointers");

[[nodiscard]] TimeUpdateSink date_time_model_sink(
    DateTimeModel& model) noexcept;

}  // namespace hmi::time
