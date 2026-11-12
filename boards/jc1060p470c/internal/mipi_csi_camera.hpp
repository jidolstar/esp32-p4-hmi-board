#pragma once

#include <array>

#include <esp_cam_ctlr.h>
#include <driver/isp_core.h>
#include <freertos/FreeRTOS.h>

#include "camera.hpp"
#include "internal/csi_frame_slots.hpp"

namespace hmi::boards::jc1060p470c::internal {

/** OV02C10 스트림과 PSRAM 슬롯 2개를 소유한다. CSI ISR과 대여자는 mux로 슬롯을 중재한다. */
class MipiCsiCamera {
 public:
  explicit MipiCsiCamera(TwoWire& bus) noexcept : bus_(bus) {}
  ~MipiCsiCamera();

  CameraStartResult start() noexcept;
  CameraFrame borrow_frame() noexcept;
  void return_frame(std::uint32_t id) noexcept;
  bool stop() noexcept;

 private:
  static bool next_buffer(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t* trans, void* context);
  static bool complete_buffer(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t* trans, void* context);
  bool write_register(std::uint16_t address, std::uint8_t value) noexcept;
  bool read_register(std::uint16_t address, std::uint8_t& value) noexcept;
  bool sensor_stream(bool enabled) noexcept;
  bool start_receiver() noexcept;
  bool balance_white() noexcept;
  bool release_awb() noexcept;
  CameraStartResult fail(CameraStartResult result) noexcept;

  TwoWire& bus_;
  esp_cam_ctlr_handle_t csi_ = nullptr;
  isp_proc_handle_t isp_ = nullptr;
  isp_awb_ctlr_t awb_ = nullptr;
  std::array<std::uint8_t*, CsiFrameSlots::kCount> buffers_{};
  CsiFrameSlots slots_{};
  portMUX_TYPE slots_mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool sensor_started_ = false;
  bool csi_enabled_ = false;
  bool csi_started_ = false;
  bool isp_enabled_ = false;
  bool awb_enabled_ = false;
  bool ccm_enabled_ = false;
  bool accepting_frames_ = false;
};

static_assert(sizeof(MipiCsiCamera) <= 128U, "CSI state must remain bounded");

}  // namespace hmi::boards::jc1060p470c::internal
