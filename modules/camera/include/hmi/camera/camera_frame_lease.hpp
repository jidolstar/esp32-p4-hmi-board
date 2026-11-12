#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace hmi::camera {

/** 카메라가 대여한 little-endian RGB565 프레임의 읽기 전용 뷰다. */
struct CameraFrameView {
  const std::uint8_t* pixels = nullptr;
  std::size_t bytes = 0U;
  std::uint32_t frame_id = 0U;
  std::uint16_t width = 0U;
  std::uint16_t height = 0U;
  std::size_t stride_bytes = 0U;
};

/** 프레임 반환 책임을 단독으로 소유한다. 포트와 반환 문맥은 lease보다 오래 살아야 한다. */
class CameraFrameLease {
 public:
  using ReturnFrame = void (*)(void* context, std::uint32_t frame_id) noexcept;

  CameraFrameLease() noexcept = default;
  CameraFrameLease(CameraFrameView view, void* context, ReturnFrame return_frame) noexcept
      : view_(return_frame ? view : CameraFrameView{}),
        context_(context), return_frame_(return_frame) {}
  ~CameraFrameLease() { reset(); }

  CameraFrameLease(const CameraFrameLease&) = delete;
  CameraFrameLease& operator=(const CameraFrameLease&) = delete;

  CameraFrameLease(CameraFrameLease&& other) noexcept
      : view_(std::exchange(other.view_, CameraFrameView{})),
        context_(std::exchange(other.context_, nullptr)),
        return_frame_(std::exchange(other.return_frame_, nullptr)) {}

  CameraFrameLease& operator=(CameraFrameLease&& other) noexcept {
    if (this != &other) {
      reset();
      view_ = std::exchange(other.view_, CameraFrameView{});
      context_ = std::exchange(other.context_, nullptr);
      return_frame_ = std::exchange(other.return_frame_, nullptr);
    }
    return *this;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return return_frame_ != nullptr; }

  /** 뷰는 현재 lease가 유지되는 동안만 읽을 수 있다. */
  [[nodiscard]] const CameraFrameView& view() const noexcept { return view_; }

  void reset() noexcept {
    const auto callback = std::exchange(return_frame_, nullptr);
    void* const context = std::exchange(context_, nullptr);
    const auto view = std::exchange(view_, CameraFrameView{});
    if (callback) {
      callback(context, view.frame_id);
    }
  }

 private:
  CameraFrameView view_{};
  void* context_ = nullptr;
  ReturnFrame return_frame_ = nullptr;
};

static_assert(sizeof(CameraFrameLease) <= 64U, "camera lease must remain bounded");

}  // namespace hmi::camera
