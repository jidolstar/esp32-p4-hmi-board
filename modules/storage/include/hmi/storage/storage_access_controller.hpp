#pragma once

#include <cstdint>

#include "hmi/storage/storage_access_port.hpp"

namespace hmi::storage {

enum class StorageAccessState : std::uint8_t {
  unavailable,
  hmi_ready,
  quiescing,
  exposing,
  waiting_for_host,
  pc_active,
  reclaiming,
  remount_failed,
};

enum class StorageAccessError : std::uint8_t {
  none,
  no_media,
  busy,
  msc_start_failed,
  callback_drain_timeout,
  remount_failed,
  port_rejected,
};

enum class StorageReclaimReason : std::uint8_t { explicit_request, host_eject, usb_unplug };

struct StorageAccessSnapshot {
  std::uint64_t total_bytes = 0U;
  std::uint64_t free_bytes = 0U;
  std::uint32_t generation = 0U;
  std::uint32_t rescan_revision = 0U;
  StorageAccessState state = StorageAccessState::unavailable;
  StorageAccessError error = StorageAccessError::none;
  StorageOwner owner = StorageOwner::none;
  StorageReclaimReason reclaim_reason = StorageReclaimReason::explicit_request;
  bool media_present = false;
  bool filesystem_mounted = false;
  bool host_seen = false;
  bool host_active = false;
  bool host_ejected = false;
  bool usb_disconnected = false;
  bool suspended = false;
  bool lock_required = false;
};

/** platform port 결과와 USB event를 HMI/Windows 공통 전이 정책으로 정규화한다. */
class StorageAccessController {
 public:
  explicit StorageAccessController(StorageAccessPort& port) noexcept
      : port_(port) {}

  bool request_pc_access(std::uint32_t now_ms) noexcept;
  bool request_hmi_access(std::uint32_t now_ms,
                          StorageReclaimReason reason) noexcept;
  bool retry_remount(std::uint32_t now_ms) noexcept;
  /** SD 미장착 상태에서 앱을 잠그지 않고 mount 가능 여부를 다시 확인한다. */
  bool retry_media(std::uint32_t now_ms) noexcept;
  void tick(std::uint32_t now_ms) noexcept;
  [[nodiscard]] StorageAccessSnapshot snapshot() const noexcept;

 private:
  void refresh(const StoragePortSnapshot& port_snapshot) noexcept;
  void dispatch(StoragePortOperation operation) noexcept;
  void handle_completion(const StoragePortSnapshot& port_snapshot) noexcept;
  void begin_reclaim(StorageReclaimReason reason) noexcept;
  static bool time_reached(std::uint32_t now_ms,
                           std::uint32_t deadline_ms) noexcept;
  static bool lock_required(StorageAccessState state) noexcept;

  StorageAccessPort& port_;
  StorageAccessSnapshot snapshot_{};
  StoragePortOperation pending_operation_ = StoragePortOperation::none;
  std::uint32_t operation_token_ = 0U;
  std::uint32_t observed_completion_revision_ = 0U;
  std::uint32_t disconnect_deadline_ms_ = 0U;
  bool initialized_ = false;
  bool operation_dispatched_ = false;
  bool disconnect_grace_active_ = false;
  bool media_probe_ = false;
};

static_assert(sizeof(StorageAccessSnapshot) <= 64U,
              "storage controller snapshot must remain at most 64 bytes");
static_assert(sizeof(StorageAccessController) <= 128U,
              "storage controller state must remain bounded");

}  // namespace hmi::storage
