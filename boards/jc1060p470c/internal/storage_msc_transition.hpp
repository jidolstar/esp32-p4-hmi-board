#pragma once

#include <cstdint>

#include "../storage.hpp"

namespace hmi::boards::jc1060p470c::internal {

/** Board의 HMI file owner → USB raw owner 전환 순서를 test 가능한 형태로 고정한다. */
template <typename StorageBackend, typename MscDriver>
UsbMscEnterResult enter_storage_usb_msc(StorageBackend& storage,
                                        MscDriver& driver) noexcept {
  if (driver.active()) {
    return UsbMscEnterResult::already_active;
  }
  if (!storage.try_begin_raw_access()) {
    return storage.ready() ? UsbMscEnterResult::busy
                           : UsbMscEnterResult::unavailable;
  }
  if (!driver.begin()) {
    storage.end_raw_access();
    return UsbMscEnterResult::start_failed;
  }
  return UsbMscEnterResult::success;
}

/** callback drain → raw 해제 → VFS 종료 → 재마운트 순서를 보장한다. */
template <typename StorageBackend, typename MscDriver>
UsbMscExitResult exit_storage_usb_msc(StorageBackend& storage,
                                      MscDriver& driver,
                                      std::uint32_t drain_timeout_ms) noexcept {
  if (!driver.active()) {
    return UsbMscExitResult::not_active;
  }
  if (!driver.end(drain_timeout_ms)) {
    return UsbMscExitResult::callback_drain_timeout;
  }
  storage.end_raw_access();
  storage.end();
  return storage.begin() ? UsbMscExitResult::success
                         : UsbMscExitResult::remount_failed;
}

}  // namespace hmi::boards::jc1060p470c::internal
