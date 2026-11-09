#pragma once

#include <cstdint>

#include "hmi/storage/storage_access_port.hpp"

namespace hmi::platform::windows::storage {

/** 실제 PC drive를 건드리지 않고 SD/USB 결과와 event를 재현하는 결정적 fixture다. */
class VirtualStorageAccess final : public ::hmi::storage::StorageAccessPort {
 public:
  VirtualStorageAccess() noexcept;

  bool request_expose(std::uint32_t token) noexcept override;
  bool request_reclaim(std::uint32_t token) noexcept override;
  bool request_remount(std::uint32_t token) noexcept override;
  void tick(std::uint32_t now_ms) noexcept override;
  [[nodiscard]] ::hmi::storage::StoragePortSnapshot snapshot()
      const noexcept override;

  void set_auto_complete(bool enabled) noexcept { auto_complete_ = enabled; }
  void set_next_result(::hmi::storage::StoragePortResult result) noexcept {
    next_result_ = result;
  }
  bool complete_pending() noexcept;
  void inject_completion(::hmi::storage::StoragePortOperation operation,
                         std::uint32_t token,
                         ::hmi::storage::StoragePortResult result) noexcept;
  void set_media_present(bool present) noexcept;
  void host_read() noexcept;
  void host_eject() noexcept;
  void usb_unplug() noexcept;
  void usb_suspend() noexcept;
  void usb_resume() noexcept;

 private:
  bool request(::hmi::storage::StoragePortOperation operation,
               std::uint32_t token) noexcept;
  void apply_completion(::hmi::storage::StoragePortOperation operation,
                        ::hmi::storage::StoragePortResult result) noexcept;

  ::hmi::storage::StoragePortSnapshot snapshot_{};
  ::hmi::storage::StoragePortResult next_result_ =
      ::hmi::storage::StoragePortResult::success;
  ::hmi::storage::StoragePortOperation pending_operation_ =
      ::hmi::storage::StoragePortOperation::none;
  std::uint32_t pending_token_ = 0U;
  bool pending_ = false;
  bool auto_complete_ = false;
};

static_assert(sizeof(VirtualStorageAccess) <= 128U,
              "virtual storage fixture must remain bounded");

}  // namespace hmi::platform::windows::storage
