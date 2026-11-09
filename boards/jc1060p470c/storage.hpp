#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace hmi::boards::jc1060p470c {

namespace internal {
class AudioService;
class SdStorage;
}  // namespace internal

enum class StorageOwner : std::uint8_t { none, hmi, usb_msc };

/** UI와 platform adapter가 복사해 사용할 수 있는 bounded 저장소 snapshot이다. */
struct StorageStatus {
  bool mounted = false;
  std::uint64_t total_bytes = 0U;
  std::uint64_t free_bytes = 0U;
  std::uint32_t generation = 0U;
  StorageOwner owner = StorageOwner::none;
};

/** Board USB callback 상태를 UI 독립 값으로 전달하는 bounded snapshot이다. */
struct UsbMscStatus {
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

enum class UsbMscExitResult : std::uint8_t {
  success,
  not_active,
  callback_drain_timeout,
  remount_failed,
};

enum class UsbMscEnterResult : std::uint8_t {
  success,
  already_active,
  unavailable,
  busy,
  start_failed,
};

/**
 * Board-owned SD 구현을 파일 경로 기반 API로만 공개하는 비소유 facade다.
 *
 * 각 파일 연산은 짧은 lease를 자동 취득한다. 여러 연산 또는 열린 stream을 하나의 작업으로
 * 보호해야 하는 service는 ExclusiveLease를 추가로 유지해 USB raw 전환을 막는다.
 */
class Storage {
 public:
  class ExclusiveLease {
   public:
    ExclusiveLease() = default;
    ~ExclusiveLease();
    ExclusiveLease(ExclusiveLease&& other) noexcept;
    ExclusiveLease& operator=(ExclusiveLease&& other) noexcept;
    ExclusiveLease(const ExclusiveLease&) = delete;
    ExclusiveLease& operator=(const ExclusiveLease&) = delete;
    explicit operator bool() const noexcept { return storage_ != nullptr; }

   private:
    friend class Storage;
    explicit ExclusiveLease(internal::SdStorage* storage) noexcept
        : storage_(storage) {}
    void release() noexcept;
    internal::SdStorage* storage_ = nullptr;
  };

  Storage(const Storage&) = delete;
  Storage& operator=(const Storage&) = delete;

  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] StorageStatus status() const noexcept;
  [[nodiscard]] std::uint32_t generation() const noexcept;
  [[nodiscard]] bool exists(const char* path) const noexcept;
  [[nodiscard]] std::uint64_t free_bytes() const noexcept;
  bool file_size(const char* path, std::uint64_t& size) const noexcept;
  bool read_file_at(const char* path, std::uint64_t offset,
                    std::uint8_t* buffer, std::size_t capacity,
                    std::size_t& bytes_read) const noexcept;
  bool write_file(const char* path, const std::uint8_t* data,
                  std::size_t size) noexcept;
  bool append_file(const char* path, const std::uint8_t* data,
                   std::size_t size) noexcept;
  bool truncate_file(const char* path, std::uint64_t size) noexcept;
  bool mkdirs(const char* path) noexcept;
  bool remove_file(const char* path) noexcept;
  bool remove_tree(const char* path) noexcept;
  bool rename_path(const char* from, const char* to) noexcept;
  [[nodiscard]] ExclusiveLease acquire_exclusive_lease() noexcept;

 private:
  friend class Board;
  friend class internal::AudioService;
  explicit Storage(internal::SdStorage& storage) noexcept : storage_(&storage) {}

  /** 호출자가 ExclusiveLease를 보유하는 streaming 전용 진입점이다. */
  std::FILE* open_read_stream(const char* path) const noexcept;
  internal::SdStorage* storage_;
};

static_assert(sizeof(StorageStatus) <= 32U,
              "StorageStatus must remain a small value snapshot");
static_assert(sizeof(UsbMscStatus) <= 32U,
              "UsbMscStatus must remain a small value snapshot");
static_assert(sizeof(Storage) == sizeof(void*),
              "Storage facade must remain pointer-sized");

}  // namespace hmi::boards::jc1060p470c
