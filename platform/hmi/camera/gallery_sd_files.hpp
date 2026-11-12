#pragma once

#include <array>

#include "boards/jc1060p470c/storage.hpp"
#include "hmi/camera/camera_gallery_ports.hpp"

namespace hmi::platform::hmi_host::camera {

/** 갤러리 worker가 사용하는 동기 SD 파일 연산이다. 호출은 하나의 worker로 직렬화한다. */
class GallerySdFiles {
 public:
  static constexpr std::size_t kMaxJpegBytes = 1024U * 1024U;

  /** profile_root는 host가 생성된 저장소 경로에서 전달한다. 경로는 생성 중 복사한다. */
  GallerySdFiles(::hmi::boards::jc1060p470c::Storage& storage, const char* profile_root) noexcept;
  GallerySdFiles(const GallerySdFiles&) = delete;
  GallerySdFiles& operator=(const GallerySdFiles&) = delete;

  ::hmi::camera::GalleryError scan(std::size_t page_index, ::hmi::camera::GalleryCompletion& result) noexcept;
  ::hmi::camera::GalleryError save_jpeg(const std::uint8_t* jpeg, std::size_t size,
                                       ::hmi::camera::PhotoId& saved_id) noexcept;
  ::hmi::camera::GalleryError read_jpeg(::hmi::camera::PhotoId id, std::uint8_t* buffer,
                                       std::size_t capacity, std::size_t& bytes_read) noexcept;
  ::hmi::camera::GalleryError erase(::hmi::camera::PhotoId id) noexcept;

 private:
  using Path = std::array<char, 240U>;
  static constexpr std::size_t kRecoveryBatchSize = 8U;
  bool photo_path(::hmi::camera::PhotoId id, Path& output, bool temporary = false) const noexcept;
  bool scan_directory() noexcept;
  static void index_file(void* context, std::string_view name, std::uint64_t bytes) noexcept;
  ::hmi::camera::GalleryError lease_error() const noexcept;

  ::hmi::boards::jc1060p470c::Storage& storage_;
  std::array<char, 192U> root_{};
  ::hmi::camera::GalleryIndex index_{};
  std::array<::hmi::camera::PhotoId, kRecoveryBatchSize> recovery_ids_{};
  std::size_t recovery_count_ = 0U;
  ::hmi::camera::PhotoId last_id_ = 0U;
};

static_assert(sizeof(GallerySdFiles) <= 1760U, "gallery file metadata must remain bounded");

}  // namespace hmi::platform::hmi_host::camera
