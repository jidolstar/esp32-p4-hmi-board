#include "camera/virtual_camera_port.hpp"

#include <algorithm>
#include <utility>

namespace hmi::platform::windows::camera {

using hmi::camera::CameraError;
using hmi::camera::CameraFrameLease;

bool VirtualCameraPort::request(Work work, std::uint32_t token) noexcept {
  if (token == 0U || work_ != Work::none || completion_.token != 0U) {
    return false;
  }
  work_ = work;
  request_token_ = token;
  return true;
}

bool VirtualCameraPort::request_start(std::uint32_t token) noexcept {
  return !running_ && request(Work::start, token);
}

bool VirtualCameraPort::request_capture(std::uint32_t token) noexcept {
  return running_ && request(Work::capture, token);
}

void VirtualCameraPort::advance() noexcept {
  if (work_ == Work::none || completion_.token != 0U) {
    return;
  }
  const auto work = std::exchange(work_, Work::none);
  completion_.token = std::exchange(request_token_, 0U);
  completion_.error = std::exchange(next_error_, CameraError::none);
  if (completion_.error != CameraError::none) {
    return;
  }
  if (work == Work::start) {
    running_ = true;
  } else {
    completion_.frame = borrow();
    if (!completion_.frame) {
      completion_.error = CameraError::frame_timeout;
    }
  }
}

bool VirtualCameraPort::inject_completion(hmi::camera::CameraCompletion completion) noexcept {
  if (completion.token == 0U || completion_.token != 0U) {
    return false;
  }
  completion_ = std::move(completion);
  return true;
}

bool VirtualCameraPort::take_completion(hmi::camera::CameraCompletion& output) noexcept {
  if (completion_.token == 0U) {
    return false;
  }
  output = std::exchange(completion_, {});
  return true;
}

CameraFrameLease VirtualCameraPort::acquire_preview() noexcept {
  return running_ && work_ == Work::none ? borrow() : CameraFrameLease{};
}

CameraFrameLease VirtualCameraPort::borrow() noexcept {
  const auto slot = std::find_if(slots_.begin(), slots_.end(), [](const Slot& item) {
    return !item.borrowed;
  });
  if (slot == slots_.end()) {
    return {};
  }
  // 대여 중인 슬롯과 겹치는 ID를 건너뛰어 uint32 wrap 뒤에도 반환 대상을 구분한다.
  do {
    ++frame_id_;
  } while (std::any_of(slots_.begin(), slots_.end(), [this](const Slot& item) {
    return item.borrowed && item.id == frame_id_;
  }));
  slot->id = frame_id_;
  slot->borrowed = true;
  for (std::size_t offset = 0U; offset < slot->pixels.size(); ++offset) {
    slot->pixels[offset] = static_cast<std::uint8_t>(slot->id + offset);
  }
  return {{slot->pixels.data(), slot->pixels.size(), slot->id, kWidth, kHeight, kWidth * 2U},
          this, release};
}

void VirtualCameraPort::release(void* context, std::uint32_t id) noexcept {
  auto& self = *static_cast<VirtualCameraPort*>(context);
  const auto slot = std::find_if(self.slots_.begin(), self.slots_.end(), [id](const Slot& item) {
    return item.borrowed && item.id == id;
  });
  if (slot != self.slots_.end()) {
    slot->borrowed = false;
  }
}

void VirtualCameraPort::stop() noexcept {
  work_ = Work::none;
  request_token_ = 0U;
  completion_ = {};
  running_ = false;
}

std::size_t VirtualCameraPort::outstanding() const noexcept {
  return static_cast<std::size_t>(std::count_if(slots_.begin(), slots_.end(), [](const Slot& item) {
    return item.borrowed;
  }));
}

}  // namespace hmi::platform::windows::camera
