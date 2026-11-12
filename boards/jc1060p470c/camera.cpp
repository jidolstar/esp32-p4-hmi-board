#include "camera.hpp"

#include <new>

#include "internal/mipi_csi_camera.hpp"

namespace hmi::boards::jc1060p470c {

Camera::Camera(TwoWire& bus) noexcept : bus_(bus) {}
Camera::~Camera() = default;

CameraStartResult Camera::start() noexcept {
  if (!driver_) {
    driver_.reset(new (std::nothrow) internal::MipiCsiCamera(bus_));
  }
  return driver_ ? driver_->start() : CameraStartResult::out_of_memory;
}

CameraFrame Camera::borrow_frame() noexcept {
  return driver_ ? driver_->borrow_frame() : CameraFrame{};
}

void Camera::return_frame(std::uint32_t id) noexcept {
  if (driver_) {
    driver_->return_frame(id);
  }
}

bool Camera::stop() noexcept {
  return !driver_ || driver_->stop();
}

}  // namespace hmi::boards::jc1060p470c
