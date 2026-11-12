#pragma once

#include <array>
#include <string_view>

#include "camera/virtual_camera_port.hpp"

namespace hmi::platform::windows::camera {

/** 메모리 안에서 SD 파일의 임시 저장, 최종 이름 전환과 뷰어 대여를 재현한다. */
class VirtualGalleryStorage final : public hmi::camera::GalleryStoragePort {
 public:
  enum class FileKind : std::uint8_t { photo, corrupt, non_regular };

  VirtualGalleryStorage() = default;
  ~VirtualGalleryStorage() override { cancel_and_drain(); }
  VirtualGalleryStorage(const VirtualGalleryStorage&) = delete;
  VirtualGalleryStorage& operator=(const VirtualGalleryStorage&) = delete;

  bool request_scan(std::uint32_t token, std::size_t page_index) noexcept override;
  bool request_save(std::uint32_t token, hmi::camera::CameraFrameLease frame) noexcept override;
  bool request_open(std::uint32_t token, hmi::camera::PhotoId id) noexcept override;
  bool request_delete(std::uint32_t token, hmi::camera::PhotoId id) noexcept override;
  bool take_completion(hmi::camera::GalleryCompletion& output) noexcept override;
  void cancel_and_drain() noexcept override;

  void advance() noexcept;
  bool inject_completion(hmi::camera::GalleryCompletion completion) noexcept;
  void fail_next(hmi::camera::GalleryError error) noexcept { next_error_ = error; }
  /** 실제 파일시스템에 접근하지 않고 기존 이름과 파일 종류를 재현한다. */
  bool seed(std::string_view filename, FileKind kind = FileKind::photo) noexcept;
  [[nodiscard]] bool exists(std::string_view filename) const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool has_viewer_lease() const noexcept { return viewer_borrowed_; }

 private:
  enum class Work : std::uint8_t { none, scan, save, open, erase };
  static constexpr std::size_t kFileCapacity = 64U;
  using Filename = std::array<char, hmi::camera::PhotoFilename{}.size() + 4U>;
  struct File {
    Filename name{};
    VirtualCameraPort::Pixels pixels{};
    std::uint32_t bytes = 0U;
    FileKind kind = FileKind::photo;
  };

  bool request(Work work, std::uint32_t token) noexcept;
  File* find(std::string_view filename) noexcept;
  hmi::camera::GalleryError scan() noexcept;
  hmi::camera::GalleryError stage_write() noexcept;
  hmi::camera::GalleryError publish_write() noexcept;
  hmi::camera::GalleryError open() noexcept;
  hmi::camera::GalleryError erase() noexcept;
  void finish(hmi::camera::GalleryError error) noexcept;
  static void release_viewer(void* context, std::uint32_t) noexcept;

  std::array<File, kFileCapacity> files_{};
  hmi::camera::GalleryIndex index_{};
  hmi::camera::GalleryCompletion completion_{};
  hmi::camera::CameraFrameLease frame_{};
  VirtualCameraPort::Pixels viewer_pixels_{};
  File* staged_ = nullptr;
  hmi::camera::PhotoId id_ = 0U;
  hmi::camera::PhotoId last_id_ = 0U;
  std::size_t page_index_ = 0U;
  std::uint32_t request_token_ = 0U;
  hmi::camera::GalleryError next_error_ = hmi::camera::GalleryError::none;
  Work work_ = Work::none;
  bool viewer_borrowed_ = false;
};

}  // namespace hmi::platform::windows::camera
