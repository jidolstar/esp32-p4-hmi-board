#include <cassert>
#include <cstdint>

#include "boards/jc1060p470c/internal/storage_msc_transition.hpp"

namespace {

struct FakeStorage {
  bool ready_value = true;
  bool raw_result = true;
  bool mount_result = true;
  bool raw_active = false;
  unsigned raw_end_count = 0U;
  unsigned unmount_count = 0U;
  unsigned mount_count = 0U;

  bool ready() const { return ready_value; }
  bool try_begin_raw_access() {
    raw_active = raw_result;
    return raw_result;
  }
  void end_raw_access() {
    raw_active = false;
    ++raw_end_count;
  }
  void end() { ++unmount_count; }
  bool begin() {
    ++mount_count;
    return mount_result;
  }
};

struct FakeDriver {
  bool active_value = false;
  bool begin_result = true;
  bool end_result = true;
  unsigned begin_count = 0U;
  unsigned end_count = 0U;
  std::uint32_t timeout = 0U;

  bool active() const { return active_value; }
  bool begin() {
    ++begin_count;
    active_value = begin_result;
    return begin_result;
  }
  bool end(std::uint32_t value) {
    ++end_count;
    timeout = value;
    if (end_result) {
      active_value = false;
    }
    return end_result;
  }
};

}  // namespace

int main() {
  using namespace hmi::boards::jc1060p470c;
  using internal::enter_storage_usb_msc;
  using internal::exit_storage_usb_msc;

  FakeStorage storage;
  FakeDriver driver;
  storage.raw_result = false;
  assert(enter_storage_usb_msc(storage, driver) == UsbMscEnterResult::busy);
  assert(driver.begin_count == 0U);

  storage.ready_value = false;
  assert(enter_storage_usb_msc(storage, driver) ==
         UsbMscEnterResult::unavailable);

  storage.ready_value = true;
  storage.raw_result = true;
  driver.begin_result = false;
  assert(enter_storage_usb_msc(storage, driver) ==
         UsbMscEnterResult::start_failed);
  assert(!storage.raw_active);
  assert(storage.raw_end_count == 1U);

  driver.begin_result = true;
  assert(enter_storage_usb_msc(storage, driver) == UsbMscEnterResult::success);
  assert(storage.raw_active);
  driver.end_result = false;
  assert(exit_storage_usb_msc(storage, driver, 500U) ==
         UsbMscExitResult::callback_drain_timeout);
  assert(storage.raw_active);
  assert(storage.unmount_count == 0U);

  driver.end_result = true;
  storage.mount_result = false;
  assert(exit_storage_usb_msc(storage, driver, 500U) ==
         UsbMscExitResult::remount_failed);
  assert(!storage.raw_active);
  assert(storage.unmount_count == 1U);
  assert(storage.mount_count == 1U);

  driver.active_value = true;
  storage.mount_result = true;
  assert(exit_storage_usb_msc(storage, driver, 250U) ==
         UsbMscExitResult::success);
  assert(driver.timeout == 250U);
  return 0;
}
