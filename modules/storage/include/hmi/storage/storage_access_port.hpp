#pragma once

#include <cstdint>

namespace hmi::storage {

enum class StorageOwner : std::uint8_t { none, hmi, usb_msc };
enum class StoragePortOperation : std::uint8_t { none, expose, reclaim, remount };
enum class StoragePortResult : std::uint8_t {
  none,
  in_progress,
  success,
  unavailable,
  busy,
  start_failed,
  callback_drain_timeout,
  remount_failed,
};

/** 공통 controller가 platform 작업과 USB callback 상태를 읽는 작은 snapshot이다. */
struct StoragePortSnapshot {
  std::uint64_t total_bytes = 0U;
  std::uint64_t free_bytes = 0U;
  std::uint32_t generation = 0U;
  std::uint32_t event_revision = 0U;
  std::uint32_t in_flight = 0U;
  std::uint32_t operation_token = 0U;
  std::uint32_t completion_revision = 0U;
  StorageOwner owner = StorageOwner::none;
  StoragePortOperation operation = StoragePortOperation::none;
  StoragePortResult result = StoragePortResult::none;
  bool mounted = false;
  bool media_present = false;
  bool host_seen = false;
  bool host_active = false;
  bool host_ejected = false;
  bool usb_started = false;
  bool usb_stopped = false;
  bool suspended = false;
  bool io_error = false;
};

class StorageAccessPort {
 public:
  virtual ~StorageAccessPort() = default;
  virtual bool request_expose(std::uint32_t token) noexcept = 0;
  virtual bool request_reclaim(std::uint32_t token) noexcept = 0;
  virtual bool request_remount(std::uint32_t token) noexcept = 0;
  virtual void tick(std::uint32_t now_ms) noexcept = 0;
  [[nodiscard]] virtual StoragePortSnapshot snapshot() const noexcept = 0;
};

static_assert(sizeof(StoragePortSnapshot) <= 64U,
              "storage port snapshot must remain at most 64 bytes");

}  // namespace hmi::storage
