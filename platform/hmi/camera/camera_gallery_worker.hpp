#pragma once

#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "boards/jc1060p470c/camera.hpp"
#include "camera/gallery_jpeg.hpp"

namespace hmi::platform::hmi_host::camera {

/** 한 worker가 CSI 설정, JPEG 및 SD I/O를 수행하고 host에는 작은 완료를 전달한다. */
class CameraGalleryWorker final : public ::hmi::camera::CameraPort, public ::hmi::camera::GalleryStoragePort {
 public:
  CameraGalleryWorker(::hmi::boards::jc1060p470c::Camera& camera,
                      ::hmi::boards::jc1060p470c::Storage& storage, const char* root) noexcept
      : camera_(camera), files_(storage, root), jpeg_(files_) {}
  ~CameraGalleryWorker() override;
  CameraGalleryWorker(const CameraGalleryWorker&) = delete;
  CameraGalleryWorker& operator=(const CameraGalleryWorker&) = delete;

  bool begin() noexcept;
  bool request_start(std::uint32_t token) noexcept override;
  bool request_capture(std::uint32_t token) noexcept override;
  bool take_completion(::hmi::camera::CameraCompletion& output) noexcept override;
  ::hmi::camera::CameraFrameLease acquire_preview() noexcept override;
  void stop() noexcept override;

  bool request_scan(std::uint32_t token, std::size_t page_index) noexcept override;
  bool request_save(std::uint32_t token, ::hmi::camera::CameraFrameLease frame) noexcept override;
  bool request_open(std::uint32_t token, ::hmi::camera::PhotoId id) noexcept override;
  bool request_delete(std::uint32_t token, ::hmi::camera::PhotoId id) noexcept override;
  bool take_completion(::hmi::camera::GalleryCompletion& output) noexcept override;
  void cancel_and_drain() noexcept override;

 private:
  enum class CameraOp : std::uint8_t { none, start, capture };
  enum class FileOp : std::uint8_t { none, scan, save, open, erase };
  struct CameraCommand { CameraOp op = CameraOp::none; std::uint32_t token = 0U; };
  struct FileCommand {
    FileOp op = FileOp::none;
    std::uint32_t token = 0U;
    ::hmi::camera::PhotoId id = 0U;
    std::size_t page = 0U;
    ::hmi::camera::CameraFrameLease frame{};
  };

  bool post(CameraCommand command) noexcept;
  bool post(FileCommand command) noexcept;
  ::hmi::camera::CameraFrameLease acquire_frame() noexcept;
  void drain(bool& flag) noexcept;
  void run_camera() noexcept;
  void run_files() noexcept;
  void run() noexcept;
  static void entry(void* context);
  static void return_frame(void* context, std::uint32_t id) noexcept;

  ::hmi::boards::jc1060p470c::Camera& camera_;
  GallerySdFiles files_;
  GalleryJpeg jpeg_;
  // 표시 루프만 갱신한다. worker에서 대여하는 촬영 프레임은 FPS 집계에서 제외된다.
  TickType_t preview_since_ = 0U;
  std::uint32_t preview_samples_ = 0U;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  TaskHandle_t task_ = nullptr;
  CameraCommand camera_command_{};
  FileCommand file_command_{};
  ::hmi::camera::CameraCompletion camera_done_{};
  ::hmi::camera::GalleryCompletion file_done_{};
  std::atomic<bool> preview_ready_{false};
  bool camera_busy_ = false;
  bool file_busy_ = false;
  bool stopping_ = false;
  bool cancelling_ = false;
  bool closing_ = false;
};

static_assert(sizeof(CameraGalleryWorker) <= 2336U, "camera worker mailboxes must remain bounded");

}  // namespace hmi::platform::hmi_host::camera
