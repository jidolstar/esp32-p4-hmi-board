#pragma once

#include "hmi/camera/camera_frame_lease.hpp"
#include "hmi/camera/gallery_index.hpp"

namespace hmi::camera {

enum class CameraError : std::uint8_t {
  none, unsupported, probe_failed, stream_failed, frame_timeout, out_of_memory, format_unsupported,
};
enum class GalleryError : std::uint8_t {
  none, not_ready, pc_owner, lease_busy, full, write_failed, invalid_file, delete_failed,
};

struct CameraCompletion {
  std::uint32_t token = 0U;
  CameraError error = CameraError::none;
  CameraFrameLease frame{};
};

/** 카메라 작업을 예약하고 완료와 읽기 전용 프레임을 호출 스레드로 전달한다. */
class CameraPort {
 public:
  virtual ~CameraPort() = default;
  virtual bool request_start(std::uint32_t token) noexcept = 0;
  virtual bool request_capture(std::uint32_t token) noexcept = 0;
  virtual bool take_completion(CameraCompletion& output) noexcept = 0;
  [[nodiscard]] virtual CameraFrameLease acquire_preview() noexcept = 0;

  /** 호출자가 자신의 lease를 반환한 뒤 호출한다. 작업과 포트 내부 프레임을 정리한다. */
  virtual void stop() noexcept = 0;
};

struct GalleryCompletion {
  std::uint32_t token = 0U;
  GalleryError error = GalleryError::none;
  PhotoId saved_id = 0U;
  CameraFrameLease image{};
  GalleryPage page{};
  std::size_t page_index = 0U;
  std::size_t total = 0U;
};

/** 요청은 작업만 예약하고 포트가 I/O를 실행한다. 완료 조회에는 파일 핸들이 전달되지 않는다. */
class GalleryStoragePort {
 public:
  virtual ~GalleryStoragePort() = default;
  /** 요청 페이지가 사라졌으면 마지막 페이지를 반환한다. */
  virtual bool request_scan(std::uint32_t token, std::size_t page_index) noexcept = 0;
  /** 요청이 거절돼도 전달한 프레임은 포트가 반환한다. */
  virtual bool request_save(std::uint32_t token, CameraFrameLease frame) noexcept = 0;
  virtual bool request_open(std::uint32_t token, PhotoId id) noexcept = 0;
  virtual bool request_delete(std::uint32_t token, PhotoId id) noexcept = 0;
  virtual bool take_completion(GalleryCompletion& output) noexcept = 0;

  /** 호출자가 뷰어 lease를 반환한 뒤 호출한다. 진행 중 파일 작업과 핸들을 모두 정리한다. */
  virtual void cancel_and_drain() noexcept = 0;
};

static_assert(sizeof(GalleryCompletion) <= 320U, "gallery completion must remain bounded");

}  // namespace hmi::camera
