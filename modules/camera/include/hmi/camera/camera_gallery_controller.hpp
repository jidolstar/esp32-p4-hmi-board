#pragma once

#include "hmi/camera/camera_gallery_ports.hpp"
#include "hmi/storage/storage_access_controller.hpp"

namespace hmi::camera {

enum class CameraSession : std::uint8_t { idle, starting, previewing, capturing, viewing, failed };

struct CameraGallerySnapshot {
  PhotoId selected_id = 0U;
  PhotoId last_saved_id = 0U;
  std::uint32_t gallery_revision = 0U;
  std::uint32_t capture_token = 0U;
  std::uint32_t capture_revision = 0U;
  std::uint8_t page_index = 0U;
  std::uint8_t item_count = 0U;
  CameraSession session = CameraSession::idle;
  CameraError camera_error = CameraError::none;
  GalleryError storage_error = GalleryError::not_ready;
  bool storage_ready = false;
  bool storage_busy = false;
};

/** 페이지 흐름과 작업 토큰을 소유한다. 포트는 controller보다 오래 살아야 한다. */
class CameraGalleryController {
 public:
  using CaptureFrameCallback = void (*)(void*, const CameraFrameView&) noexcept;
  CameraGalleryController(CameraPort& camera, GalleryStoragePort& storage) noexcept
      : camera_(camera), storage_(storage) {}
  ~CameraGalleryController() { stop(); }
  CameraGalleryController(const CameraGalleryController&) = delete;
  CameraGalleryController& operator=(const CameraGalleryController&) = delete;

  /** S05 quiescing 상태를 owner 변경 전에 전달하면 저장 작업과 프레임을 먼저 정리한다. */
  void update_storage(const hmi::storage::StorageAccessSnapshot& access) noexcept;
  bool start() noexcept;
  void stop() noexcept;
  /** 촬영 프레임은 저장소에 넘기기 직전 callback 동안만 유효하다. callback에서 controller를 다시 호출하지 않는다. */
  void poll(CaptureFrameCallback on_capture = nullptr, void* context = nullptr) noexcept;

  bool capture() noexcept;
  bool refresh(std::size_t page_index = 0U) noexcept;
  bool open(PhotoId id) noexcept;
  bool back_to_preview() noexcept;
  /** 확인된 ID가 현재 뷰어의 사진과 일치할 때만 삭제를 예약한다. */
  bool delete_selected(PhotoId confirmed_id) noexcept;

  [[nodiscard]] const CameraGallerySnapshot& snapshot() const noexcept { return snapshot_; }
  [[nodiscard]] const GalleryPage& page() const noexcept { return page_; }
  /** 뷰와 픽셀은 다음 controller 호출에서 바뀔 수 있다. 렌더러가 따로 보관하지 않는다. */
  [[nodiscard]] const CameraFrameView* image() const noexcept;
  /** 미리보기를 그린 뒤 CSI가 두 DMA 슬롯을 계속 교대할 수 있도록 반환한다. */
  void release_preview() noexcept;

 private:
  enum class CameraWork : std::uint8_t { none, start, capture };
  enum class GalleryWork : std::uint8_t { none, scan, save, open, erase };

  std::uint32_t next_token() noexcept;
  bool start_preview() noexcept;
  bool scan(std::size_t page_index) noexcept;
  bool load(PhotoId id) noexcept;
  bool gallery_available() const noexcept;
  void finish_capture() noexcept;
  void cancel_gallery() noexcept;
  void clear_page() noexcept;
  void handle_camera(CameraCompletion& result, CaptureFrameCallback on_capture, void* context) noexcept;
  void handle_gallery(GalleryCompletion& result) noexcept;

  CameraPort& camera_;
  GalleryStoragePort& storage_;
  CameraGallerySnapshot snapshot_{};
  GalleryPage page_{};
  CameraFrameLease image_{};
  std::uint32_t token_ = 0U;
  std::uint32_t camera_token_ = 0U;
  std::uint32_t gallery_token_ = 0U;
  std::uint32_t storage_generation_ = 0U;
  std::uint32_t rescan_revision_ = 0U;
  std::size_t deleted_position_ = 0U;
  CameraWork camera_work_ = CameraWork::none;
  GalleryWork gallery_work_ = GalleryWork::none;
  bool active_ = false;
  bool storage_locked_ = false;
  bool select_after_scan_ = false;
};

static_assert(sizeof(CameraGallerySnapshot) <= 48U, "camera snapshot must remain bounded");
static_assert(sizeof(CameraGalleryController) <= 384U, "camera controller must remain bounded");

}  // namespace hmi::camera
