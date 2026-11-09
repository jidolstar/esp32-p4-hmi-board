#include "internal/usb_msc_driver.hpp"

#include <Arduino.h>
#include <SD_MMC.h>
#include <USB.h>
#include <USBMSC.h>

#include <cstdint>
#include <limits>

#include "internal/msc_sector_transfer.hpp"
#include "internal/usb_msc_event_state.hpp"
#include "log.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

USBMSC g_msc;
UsbMscEventState g_event_state;
bool g_usb_event_registered = false;

class CallbackFlight {
 public:
  CallbackFlight() noexcept {
    engaged_ = g_event_state.try_begin_io();
  }

  ~CallbackFlight() {
    if (engaged_) {
      g_event_state.end_io();
    }
  }

  explicit operator bool() const noexcept { return engaged_; }

 private:
  bool engaged_ = false;
};

std::int32_t on_write(std::uint32_t lba, std::uint32_t offset,
                      std::uint8_t* buffer, std::uint32_t size) {
  CallbackFlight flight;
  if (!flight) {
    return -1;
  }
  const auto result = transfer_msc_sectors(
      true, lba, offset, buffer, size,
      [](std::uint8_t* sector, std::uint32_t block) {
        return SD_MMC.readRAW(sector, block);
      },
      [](const std::uint8_t* sector, std::uint32_t block) {
        return SD_MMC.writeRAW(const_cast<std::uint8_t*>(sector), block);
      });
  if (result < 0) {
    g_event_state.set_io_error();
  }
  return result;
}

std::int32_t on_read(std::uint32_t lba, std::uint32_t offset, void* buffer,
                     std::uint32_t size) {
  CallbackFlight flight;
  if (!flight) {
    return -1;
  }
  const auto result = transfer_msc_sectors(
      false, lba, offset, buffer, size,
      [](std::uint8_t* sector, std::uint32_t block) {
        return SD_MMC.readRAW(sector, block);
      },
      [](const std::uint8_t* sector, std::uint32_t block) {
        return SD_MMC.writeRAW(const_cast<std::uint8_t*>(sector), block);
      });
  if (result < 0) {
    g_event_state.set_io_error();
  }
  return result;
}

bool on_start_stop(std::uint8_t, bool start, bool load_eject) {
  g_event_state.on_start_stop(start, load_eject);
  return true;
}

void on_usb_event(void*, esp_event_base_t, std::int32_t event_id, void*) {
  switch (event_id) {
    case ARDUINO_USB_STARTED_EVENT:
      g_event_state.on_bus_signal(UsbBusSignal::started);
      break;
    case ARDUINO_USB_STOPPED_EVENT:
      g_event_state.on_bus_signal(UsbBusSignal::stopped);
      break;
    case ARDUINO_USB_SUSPEND_EVENT:
      g_event_state.on_bus_signal(UsbBusSignal::suspended);
      break;
    case ARDUINO_USB_RESUME_EVENT:
      g_event_state.on_bus_signal(UsbBusSignal::resumed);
      break;
    default:
      return;
  }
}

}  // namespace

UsbMscDriver::~UsbMscDriver() {
  if (active()) {
    end(0U);
  }
}

bool UsbMscDriver::begin() noexcept {
  if (active()) {
    return true;
  }
  const std::uint64_t sectors = SD_MMC.numSectors() != 0U
                                    ? SD_MMC.numSectors()
                                    : SD_MMC.cardSize() / kMscSectorSize;
  if (sectors == 0U ||
      sectors > std::numeric_limits<std::uint32_t>::max()) {
    HMI_LOG_ERROR("[usb-msc] invalid SD sector count=%llu\r\n",
                  static_cast<unsigned long long>(sectors));
    return false;
  }

  alignas(4) std::uint8_t probe[kMscSectorSize]{};
  if (!SD_MMC.readRAW(probe, 0U)) {
    HMI_LOG_ERROR("[usb-msc] raw sector preflight failed\r\n");
    return false;
  }

  g_event_state.reset();
  g_msc.vendorID("CookHMI");
  g_msc.productID("HMI SD Card");
  g_msc.productRevision("1.0");
  g_msc.onRead(on_read);
  g_msc.onWrite(on_write);
  g_msc.onStartStop(on_start_stop);
  g_msc.isWritable(true);
  g_msc.mediaPresent(false);
  if (!g_msc.begin(static_cast<std::uint32_t>(sectors), kMscSectorSize)) {
    g_msc.end();
    HMI_LOG_ERROR("[usb-msc] LUN begin failed\r\n");
    return false;
  }
  if (!g_usb_event_registered) {
    USB.onEvent(on_usb_event);
    g_usb_event_registered = true;
  }
  // CDC-on-boot stack은 이미 시작됐을 수 있다. begin은 전체 stack을 끄지 않고 재사용한다.
  if (!USB.begin()) {
    g_msc.end();
    HMI_LOG_ERROR("[usb-msc] USB device stack unavailable\r\n");
    return false;
  }

  g_event_state.set_active(true);
  g_event_state.set_media_present(true);
  g_msc.mediaPresent(true);
  HMI_LOG_INFO("[usb-msc] ready sectors=%llu\r\n",
               static_cast<unsigned long long>(sectors));
  return true;
}

bool UsbMscDriver::end(std::uint32_t drain_timeout_ms) noexcept {
  if (!active()) {
    return true;
  }
  g_event_state.set_media_present(false);
  g_msc.mediaPresent(false);
  const std::uint32_t started = millis();
  while (g_event_state.snapshot().in_flight != 0U) {
    if (static_cast<std::uint32_t>(millis() - started) >= drain_timeout_ms) {
      HMI_LOG_ERROR("[usb-msc] callback drain timeout in_flight=%u\r\n",
                    static_cast<unsigned>(
                        g_event_state.snapshot().in_flight));
      return false;
    }
    delay(1U);
  }
  g_msc.end();
  g_event_state.set_active(false);
  HMI_LOG_INFO("[usb-msc] stopped without stopping USB CDC stack\r\n");
  return true;
}

bool UsbMscDriver::active() const noexcept {
  return g_event_state.snapshot().active;
}

UsbMscDriverSnapshot UsbMscDriver::snapshot() const noexcept {
  return g_event_state.snapshot();
}

}  // namespace hmi::boards::jc1060p470c::internal
