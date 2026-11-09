#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace hmi::boards::jc1060p470c::internal {

/**
 * 보드 TF 슬롯의 SDMMC mount 수명을 소유한다.
 *
 * LDO4와 SD 전원 GPIO는 Arduino esp32p4 보드 variant가 SD_MMC 내부에서
 * 관리한다. 여기서 LDO를 별도로 취득하면 동일 채널을 이중 소유하게 된다.
 * 파일 정책이나 UI 자산 경로는 알지 않으며 Board facade가 준비 상태만 공개한다.
 */
class SdStorage {
 public:
  SdStorage() = default;
  SdStorage(const SdStorage&) = delete;
  SdStorage& operator=(const SdStorage&) = delete;
  ~SdStorage();

  bool begin();
  void end() noexcept;
  [[nodiscard]] bool ready() const noexcept;

  // 아래 파일 연산은 Board가 소유한 asset updater만 사용한다.
  [[nodiscard]] bool exists(const char* path) const noexcept;
  [[nodiscard]] std::uint64_t free_bytes() const noexcept;
  bool file_size(const char* path, std::uint64_t& size) const noexcept;
  bool read_file_at(const char* path, std::uint64_t offset,
                    std::uint8_t* buffer, std::size_t capacity,
                    std::size_t& bytes_read) const noexcept;
  /** Board-owned streaming service가 사용하며 호출자가 fclose로 닫아야 한다. */
  std::FILE* open_read_stream(const char* path) const noexcept;
  bool write_file(const char* path, const std::uint8_t* data,
                  std::size_t size) noexcept;
  bool append_file(const char* path, const std::uint8_t* data,
                   std::size_t size) noexcept;
  bool truncate_file(const char* path, std::uint64_t size) noexcept;
  bool mkdirs(const char* path) noexcept;
  bool remove_file(const char* path) noexcept;
  bool remove_tree(const char* path) noexcept;
  bool rename_path(const char* from, const char* to) noexcept;

 private:
  bool resolve_path(const char* path, char* output,
                    std::size_t capacity) const noexcept;
  bool write_with_mode(const char* path, const std::uint8_t* data,
                       std::size_t size, const char* mode) noexcept;
  bool remove_tree_impl(const char* full_path, unsigned depth) noexcept;

  bool mounted_ = false;
};

}  // namespace hmi::boards::jc1060p470c::internal
