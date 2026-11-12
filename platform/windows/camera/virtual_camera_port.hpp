#pragma once

#include <array>

#include "hmi/camera/camera_gallery_ports.hpp"

namespace hmi::platform::windows::camera {

/** 두 개의 고정 RGB565 프레임과 호출자가 진행시키는 완료를 제공한다. */
class VirtualCameraPort final : public hmi::camera::CameraPort {
 public:
  static constexpr std::uint16_t kWidth = 4U;
  static constexpr std::uint16_t kHeight = 3U;
  using Pixels = std::array<std::uint8_t, kWidth * kHeight * 2U>;

  VirtualCameraPort() = default;
  VirtualCameraPort(const VirtualCameraPort&) = delete;
  VirtualCameraPort& operator=(const VirtualCameraPort&) = delete;

  bool request_start(std::uint32_t token) noexcept override;
  bool request_capture(std::uint32_t token) noexcept override;
  bool take_completion(hmi::camera::CameraCompletion& output) noexcept override;
  [[nodiscard]] hmi::camera::CameraFrameLease acquire_preview() noexcept override;
  void stop() noexcept override;

  void advance() noexcept;
  bool inject_completion(hmi::camera::CameraCompletion completion) noexcept;
  void fail_next(hmi::camera::CameraError error) noexcept { next_error_ = error; }
  [[nodiscard]] bool running() const noexcept { return running_; }
  [[nodiscard]] std::size_t outstanding() const noexcept;

 private:
  enum class Work : std::uint8_t { none, start, capture };
  struct Slot {
    Pixels pixels{};
    std::uint32_t id = 0U;
    bool borrowed = false;
  };

  bool request(Work work, std::uint32_t token) noexcept;
  hmi::camera::CameraFrameLease borrow() noexcept;
  static void release(void* context, std::uint32_t id) noexcept;

  std::array<Slot, 2U> slots_{};
  hmi::camera::CameraCompletion completion_{};
  hmi::camera::CameraError next_error_ = hmi::camera::CameraError::none;
  std::uint32_t request_token_ = 0U;
  std::uint32_t frame_id_ = 0U;
  Work work_ = Work::none;
  bool running_ = false;
};

}  // namespace hmi::platform::windows::camera
