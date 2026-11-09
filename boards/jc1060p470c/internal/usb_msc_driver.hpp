#pragma once

#include <cstdint>

namespace hmi::boards::jc1060p470c::internal {

/** USB callback이 남기는 lock-free 상태 snapshot이다. */
struct UsbMscDriverSnapshot {
  bool active = false;
  bool media_present = false;
  bool host_seen = false;
  bool host_active = false;
  bool host_ejected = false;
  bool usb_started = false;
  bool usb_stopped = false;
  bool suspended = false;
  bool io_error = false;
  std::uint32_t in_flight = 0U;
  std::uint32_t event_revision = 0U;
};

/** ESP32-P4 TinyUSB MSC와 SD raw sector callback의 수명을 캡슐화한다. */
class UsbMscDriver {
 public:
  UsbMscDriver() = default;
  ~UsbMscDriver();
  UsbMscDriver(const UsbMscDriver&) = delete;
  UsbMscDriver& operator=(const UsbMscDriver&) = delete;

  bool begin() noexcept;
  /** 새 I/O를 막고 callback drain 뒤 MSC LUN을 종료한다. timeout이면 active를 유지한다. */
  bool end(std::uint32_t drain_timeout_ms) noexcept;
  [[nodiscard]] bool active() const noexcept;
  [[nodiscard]] UsbMscDriverSnapshot snapshot() const noexcept;
};

static_assert(sizeof(UsbMscDriverSnapshot) <= 32U,
              "USB MSC callback snapshot must remain bounded");

}  // namespace hmi::boards::jc1060p470c::internal
