#include "storage/board_storage_access.hpp"

#include "board.hpp"

namespace hmi::platform::hmi_host::storage {
namespace {

constexpr std::uint32_t kCallbackDrainTimeoutMs = 500U;
constexpr std::uint32_t kWorkerStackBytes = 6144U;

::hmi::storage::StorageOwner map_owner(
    boards::jc1060p470c::StorageOwner owner) noexcept {
  switch (owner) {
    case boards::jc1060p470c::StorageOwner::hmi:
      return ::hmi::storage::StorageOwner::hmi;
    case boards::jc1060p470c::StorageOwner::usb_msc:
      return ::hmi::storage::StorageOwner::usb_msc;
    case boards::jc1060p470c::StorageOwner::none:
      return ::hmi::storage::StorageOwner::none;
  }
  return ::hmi::storage::StorageOwner::none;
}

}  // namespace

bool BoardStorageAccess::begin() noexcept {
  if (queue_ != nullptr) {
    return true;
  }
  publish_storage_status();
  queue_ = xQueueCreate(1U, sizeof(Command));
  if (queue_ == nullptr) {
    return false;
  }
  if (xTaskCreate(worker_entry, "storage_access", kWorkerStackBytes, this, 2U,
                  &task_) != pdPASS) {
    vQueueDelete(queue_);
    queue_ = nullptr;
    task_ = nullptr;
    return false;
  }
  return true;
}

bool BoardStorageAccess::request_expose(std::uint32_t token) noexcept {
  return request(::hmi::storage::StoragePortOperation::expose, token);
}

bool BoardStorageAccess::request_reclaim(std::uint32_t token) noexcept {
  return request(::hmi::storage::StoragePortOperation::reclaim, token);
}

bool BoardStorageAccess::request_remount(std::uint32_t token) noexcept {
  return request(::hmi::storage::StoragePortOperation::remount, token);
}

bool BoardStorageAccess::request(
    ::hmi::storage::StoragePortOperation operation,
    std::uint32_t token) noexcept {
  if (queue_ == nullptr || operation == ::hmi::storage::StoragePortOperation::none) {
    return false;
  }
  bool expected = false;
  if (!operation_pending_.compare_exchange_strong(
          expected, true, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }
  operation_.store(operation, std::memory_order_release);
  operation_token_.store(token, std::memory_order_release);
  result_.store(::hmi::storage::StoragePortResult::in_progress,
                std::memory_order_release);
  const Command command{operation, token};
  if (xQueueOverwrite(queue_, &command) != pdTRUE) {
    result_.store(::hmi::storage::StoragePortResult::none,
                  std::memory_order_release);
    operation_pending_.store(false, std::memory_order_release);
    return false;
  }
  return true;
}

void BoardStorageAccess::tick(std::uint32_t) noexcept {
  // USB callback은 Board snapshot에 atomic만 기록한다. blocking 작업은 worker만 수행한다.
}

::hmi::storage::StoragePortSnapshot BoardStorageAccess::snapshot()
    const noexcept {
  boards::jc1060p470c::StorageStatus storage_status{};
  taskENTER_CRITICAL(&storage_mux_);
  storage_status = storage_status_;
  taskEXIT_CRITICAL(&storage_mux_);

  const auto usb = board_.usb_msc_status();
  return {storage_status.total_bytes,
          storage_status.free_bytes,
          storage_status.generation,
          usb.event_revision,
          usb.in_flight,
          operation_token_.load(std::memory_order_acquire),
          completion_revision_.load(std::memory_order_acquire),
          map_owner(storage_status.owner),
          operation_.load(std::memory_order_acquire),
          result_.load(std::memory_order_acquire),
          storage_status.mounted,
          usb.media_present,
          usb.host_seen,
          usb.host_active,
          usb.host_ejected,
          usb.usb_started,
          usb.usb_stopped,
          usb.suspended,
          usb.io_error};
}

void BoardStorageAccess::worker_entry(void* context) {
  static_cast<BoardStorageAccess*>(context)->worker();
}

void BoardStorageAccess::worker() noexcept {
  Command command{};
  for (;;) {
    if (xQueueReceive(queue_, &command, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    auto result = ::hmi::storage::StoragePortResult::unavailable;
    if (command.operation == ::hmi::storage::StoragePortOperation::expose) {
      switch (board_.enter_usb_msc()) {
        case boards::jc1060p470c::UsbMscEnterResult::success:
        case boards::jc1060p470c::UsbMscEnterResult::already_active:
          result = ::hmi::storage::StoragePortResult::success;
          break;
        case boards::jc1060p470c::UsbMscEnterResult::unavailable:
          result = ::hmi::storage::StoragePortResult::unavailable;
          break;
        case boards::jc1060p470c::UsbMscEnterResult::busy:
          result = ::hmi::storage::StoragePortResult::busy;
          break;
        case boards::jc1060p470c::UsbMscEnterResult::start_failed:
          result = ::hmi::storage::StoragePortResult::start_failed;
          break;
      }
    } else if (command.operation ==
               ::hmi::storage::StoragePortOperation::reclaim) {
      switch (board_.exit_usb_msc(kCallbackDrainTimeoutMs)) {
        case boards::jc1060p470c::UsbMscExitResult::success:
        case boards::jc1060p470c::UsbMscExitResult::not_active:
          result = ::hmi::storage::StoragePortResult::success;
          break;
        case boards::jc1060p470c::UsbMscExitResult::callback_drain_timeout:
          result = ::hmi::storage::StoragePortResult::callback_drain_timeout;
          break;
        case boards::jc1060p470c::UsbMscExitResult::remount_failed:
          result = ::hmi::storage::StoragePortResult::remount_failed;
          break;
      }
    } else if (command.operation ==
               ::hmi::storage::StoragePortOperation::remount) {
      // callback drain timeout 뒤에는 USB/raw owner가 의도적으로 유지된다. 이때
      // retry는 mount만 반복하지 않고 MSC 종료부터 다시 수행해야 복구할 수 있다.
      if (board_.usb_msc_status().active) {
        switch (board_.exit_usb_msc(kCallbackDrainTimeoutMs)) {
          case boards::jc1060p470c::UsbMscExitResult::success:
          case boards::jc1060p470c::UsbMscExitResult::not_active:
            result = ::hmi::storage::StoragePortResult::success;
            break;
          case boards::jc1060p470c::UsbMscExitResult::callback_drain_timeout:
            result =
                ::hmi::storage::StoragePortResult::callback_drain_timeout;
            break;
          case boards::jc1060p470c::UsbMscExitResult::remount_failed:
            result = ::hmi::storage::StoragePortResult::remount_failed;
            break;
        }
      } else {
        result = board_.retry_storage_mount()
                     ? ::hmi::storage::StoragePortResult::success
                     : ::hmi::storage::StoragePortResult::remount_failed;
      }
    }
    publish_storage_status();
    operation_token_.store(command.token, std::memory_order_release);
    result_.store(result, std::memory_order_release);
    completion_revision_.fetch_add(1U, std::memory_order_acq_rel);
    operation_pending_.store(false, std::memory_order_release);
  }
}

void BoardStorageAccess::publish_storage_status() noexcept {
  const auto status = board_.storage().status();
  taskENTER_CRITICAL(&storage_mux_);
  storage_status_ = status;
  taskEXIT_CRITICAL(&storage_mux_);
}

}  // namespace hmi::platform::hmi_host::storage
