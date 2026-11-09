#pragma once

#include <atomic>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "hmi/storage/storage_access_port.hpp"
#include "storage.hpp"

namespace hmi::boards::jc1060p470c {
class Board;
}  // namespace hmi::boards::jc1060p470c

namespace hmi::platform::hmi_host::storage {

/** Board의 blocking MSC 전환을 전용 worker에서 수행하는 HMI port adapter다. */
class BoardStorageAccess final : public ::hmi::storage::StorageAccessPort {
 public:
  explicit BoardStorageAccess(boards::jc1060p470c::Board& board) noexcept
      : board_(board) {}
  BoardStorageAccess(const BoardStorageAccess&) = delete;
  BoardStorageAccess& operator=(const BoardStorageAccess&) = delete;

  bool begin() noexcept;
  bool request_expose(std::uint32_t token) noexcept override;
  bool request_reclaim(std::uint32_t token) noexcept override;
  bool request_remount(std::uint32_t token) noexcept override;
  void tick(std::uint32_t now_ms) noexcept override;
  [[nodiscard]] ::hmi::storage::StoragePortSnapshot snapshot()
      const noexcept override;

 private:
  struct Command {
    ::hmi::storage::StoragePortOperation operation =
        ::hmi::storage::StoragePortOperation::none;
    std::uint32_t token = 0U;
  };

  static void worker_entry(void* context);
  void worker() noexcept;
  bool request(::hmi::storage::StoragePortOperation operation,
               std::uint32_t token) noexcept;
  void publish_storage_status() noexcept;

  boards::jc1060p470c::Board& board_;
  QueueHandle_t queue_ = nullptr;
  TaskHandle_t task_ = nullptr;
  std::atomic<bool> operation_pending_{false};
  std::atomic<::hmi::storage::StoragePortOperation> operation_{
      ::hmi::storage::StoragePortOperation::none};
  std::atomic<::hmi::storage::StoragePortResult> result_{
      ::hmi::storage::StoragePortResult::none};
  std::atomic<std::uint32_t> operation_token_{0U};
  std::atomic<std::uint32_t> completion_revision_{0U};
  mutable portMUX_TYPE storage_mux_ = portMUX_INITIALIZER_UNLOCKED;
  boards::jc1060p470c::StorageStatus storage_status_{};
};

static_assert(sizeof(BoardStorageAccess) <= 128U,
              "HMI storage adapter must remain bounded");

}  // namespace hmi::platform::hmi_host::storage
