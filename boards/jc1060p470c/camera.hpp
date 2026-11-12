#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

class TwoWire;

namespace hmi::boards::jc1060p470c {

namespace internal { class MipiCsiCamera; }

enum class CameraStartResult : std::uint8_t { ready, probe_failed, stream_failed, out_of_memory };

/** Camera가 대여한 RGB565 LE 프레임이다. 반환 전까지 픽셀을 읽을 수 있다. */
struct CameraFrame {
  const std::uint8_t* pixels = nullptr;
  std::size_t bytes = 0U;
  std::uint32_t id = 0U;
  std::uint16_t width = 0U;
  std::uint16_t height = 0U;
  std::size_t stride_bytes = 0U;
};

/** Board의 카메라 서비스다. 시작·정지는 한 작업자가 직렬 호출하고 프레임 반환은 다른 작업자도 가능하다. */
class Camera {
 public:
  ~Camera();
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  CameraStartResult start() noexcept;
  /** 준비된 프레임을 한 번 대여한다. 준비되지 않았으면 빈 프레임을 반환한다. */
  [[nodiscard]] CameraFrame borrow_frame() noexcept;
  void return_frame(std::uint32_t id) noexcept;
  /** 모든 프레임이 돌아온 뒤 스트림과 DMA 버퍼를 정리한다. */
  bool stop() noexcept;

 private:
  friend class Board;
  explicit Camera(TwoWire& bus) noexcept;
  TwoWire& bus_;
  std::unique_ptr<internal::MipiCsiCamera> driver_;
};

}  // namespace hmi::boards::jc1060p470c
