#pragma once

#include <atomic>
#include <cstdint>

#include "usb_msc_driver.hpp"

namespace hmi::boards::jc1060p470c::internal {

enum class UsbBusSignal : std::uint8_t { started, stopped, suspended, resumed };

/** USB task callback이 blocking 작업 없이 갱신하는 atomic event state다. */
class UsbMscEventState {
 public:
  void reset() noexcept {
    active_.store(false, std::memory_order_release);
    media_present_.store(false, std::memory_order_release);
    host_seen_.store(false, std::memory_order_release);
    host_active_.store(false, std::memory_order_release);
    host_ejected_.store(false, std::memory_order_release);
    usb_started_.store(false, std::memory_order_release);
    usb_stopped_.store(false, std::memory_order_release);
    suspended_.store(false, std::memory_order_release);
    io_error_.store(false, std::memory_order_release);
    in_flight_.store(0U, std::memory_order_release);
    mark_event();
  }

  void set_active(bool active) noexcept {
    active_.store(active, std::memory_order_release);
    mark_event();
  }

  void set_media_present(bool present) noexcept {
    media_present_.store(present, std::memory_order_release);
    mark_event();
  }

  [[nodiscard]] bool try_begin_io() noexcept {
    if (!active_.load(std::memory_order_acquire) ||
        !media_present_.load(std::memory_order_acquire)) {
      return false;
    }
    in_flight_.fetch_add(1U, std::memory_order_acq_rel);
    if (!active_.load(std::memory_order_acquire) ||
        !media_present_.load(std::memory_order_acquire)) {
      in_flight_.fetch_sub(1U, std::memory_order_acq_rel);
      return false;
    }
    host_seen_.store(true, std::memory_order_release);
    host_active_.store(true, std::memory_order_release);
    mark_event();
    return true;
  }

  void end_io() noexcept {
    const std::uint32_t current = in_flight_.load(std::memory_order_acquire);
    if (current != 0U) {
      in_flight_.fetch_sub(1U, std::memory_order_acq_rel);
    }
  }

  void on_start_stop(bool start, bool load_eject) noexcept {
    if (!active_.load(std::memory_order_acquire)) {
      return;
    }
    if (start) {
      host_seen_.store(true, std::memory_order_release);
      host_active_.store(true, std::memory_order_release);
    } else {
      host_active_.store(false, std::memory_order_release);
      if (load_eject) {
        host_ejected_.store(true, std::memory_order_release);
      }
    }
    mark_event();
  }

  void on_bus_signal(UsbBusSignal signal) noexcept {
    if (!active_.load(std::memory_order_acquire)) {
      return;
    }
    switch (signal) {
      case UsbBusSignal::started:
        usb_started_.store(true, std::memory_order_release);
        usb_stopped_.store(false, std::memory_order_release);
        break;
      case UsbBusSignal::stopped:
        usb_stopped_.store(true, std::memory_order_release);
        usb_started_.store(false, std::memory_order_release);
        suspended_.store(false, std::memory_order_release);
        break;
      case UsbBusSignal::suspended:
        suspended_.store(true, std::memory_order_release);
        break;
      case UsbBusSignal::resumed:
        suspended_.store(false, std::memory_order_release);
        break;
    }
    mark_event();
  }

  void set_io_error() noexcept {
    io_error_.store(true, std::memory_order_release);
    mark_event();
  }

  [[nodiscard]] UsbMscDriverSnapshot snapshot() const noexcept {
    return {active_.load(std::memory_order_acquire),
            media_present_.load(std::memory_order_acquire),
            host_seen_.load(std::memory_order_acquire),
            host_active_.load(std::memory_order_acquire),
            host_ejected_.load(std::memory_order_acquire),
            usb_started_.load(std::memory_order_acquire),
            usb_stopped_.load(std::memory_order_acquire),
            suspended_.load(std::memory_order_acquire),
            io_error_.load(std::memory_order_acquire),
            in_flight_.load(std::memory_order_acquire),
            event_revision_.load(std::memory_order_acquire)};
  }

 private:
  void mark_event() noexcept {
    event_revision_.fetch_add(1U, std::memory_order_acq_rel);
  }

  std::atomic<bool> active_{false};
  std::atomic<bool> media_present_{false};
  std::atomic<bool> host_seen_{false};
  std::atomic<bool> host_active_{false};
  std::atomic<bool> host_ejected_{false};
  std::atomic<bool> usb_started_{false};
  std::atomic<bool> usb_stopped_{false};
  std::atomic<bool> suspended_{false};
  std::atomic<bool> io_error_{false};
  std::atomic<std::uint32_t> in_flight_{0U};
  std::atomic<std::uint32_t> event_revision_{0U};
};

static_assert(sizeof(UsbMscEventState) <= 32U,
              "USB callback event state must remain bounded");

}  // namespace hmi::boards::jc1060p470c::internal
