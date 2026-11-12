#include "internal/mipi_csi_camera.hpp"

#include <algorithm>
#include <Arduino.h>
#include <Wire.h>
#include <driver/isp_awb.h>
#include <driver/isp_ccm.h>
#include <esp_attr.h>
#include <esp_cam_ctlr_csi.h>
#include <esp_heap_caps.h>

#include "internal/i2c_transaction.hpp"
#include "internal/ov02c10_1288x728.hpp"
#include "log.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

constexpr std::uint8_t kSensorAddress = 0x36U;
constexpr std::uint16_t kWidth = 1288U;
constexpr std::uint16_t kHeight = 728U;
constexpr std::size_t kStride = kWidth * 2U;
constexpr std::size_t kFrameBytes = kStride * kHeight;
constexpr auto kBufferCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED;

bool ok(esp_err_t result, const char* operation) noexcept {
  if (result != ESP_OK) {
    HMI_LOG_ERROR("[camera] %s: %s\r\n", operation, esp_err_to_name(result));
  }
  return result == ESP_OK;
}

}  // namespace

MipiCsiCamera::~MipiCsiCamera() { stop(); }

bool MipiCsiCamera::write_register(std::uint16_t address, std::uint8_t value) noexcept {
  const I2cTransaction transaction(30U);
  if (!transaction.acquired()) {
    return false;
  }
  bus_.beginTransmission(kSensorAddress);
  bus_.write(static_cast<std::uint8_t>(address >> 8U));
  bus_.write(static_cast<std::uint8_t>(address));
  bus_.write(value);
  return bus_.endTransmission() == 0U;
}

bool MipiCsiCamera::read_register(std::uint16_t address, std::uint8_t& value) noexcept {
  const I2cTransaction transaction(30U);
  if (!transaction.acquired()) {
    return false;
  }
  bus_.beginTransmission(kSensorAddress);
  bus_.write(static_cast<std::uint8_t>(address >> 8U));
  bus_.write(static_cast<std::uint8_t>(address));
  if (bus_.endTransmission(false) != 0U || bus_.requestFrom(kSensorAddress, std::size_t{1}) != 1U) {
    return false;
  }
  const auto byte = bus_.read();
  if (byte < 0) {
    return false;
  }
  value = static_cast<std::uint8_t>(byte);
  return true;
}

bool MipiCsiCamera::sensor_stream(bool enabled) noexcept {
  const bool pad = write_register(0x4800U, enabled ? 0x64U : 0x00U);
  const bool sleep = write_register(0x0100U, enabled ? 0x01U : 0x00U);
  return pad && sleep;
}

bool IRAM_ATTR MipiCsiCamera::next_buffer(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t* trans, void* context) {
  auto& self = *static_cast<MipiCsiCamera*>(context);
  portENTER_CRITICAL_SAFE(&self.slots_mux_);
  trans->buffer = self.buffers_[self.slots_.next_dma()];
  trans->buflen = kFrameBytes;
  portEXIT_CRITICAL_SAFE(&self.slots_mux_);
  return false;
}

bool IRAM_ATTR MipiCsiCamera::complete_buffer(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t* trans, void* context) {
  auto& self = *static_cast<MipiCsiCamera*>(context);
  portENTER_CRITICAL_ISR(&self.slots_mux_);
  for (std::uint8_t index = 0U; index < self.buffers_.size(); ++index) {
    if (self.buffers_[index] == trans->buffer) {
      self.slots_.complete(index, trans->received_size == kFrameBytes);
      break;
    }
  }
  portEXIT_CRITICAL_ISR(&self.slots_mux_);
  return false;
}

bool MipiCsiCamera::start_receiver() noexcept {
  esp_cam_ctlr_evt_cbs_t callbacks{};
  callbacks.on_get_new_trans = next_buffer;
  callbacks.on_trans_finished = complete_buffer;
  if (!ok(esp_cam_ctlr_register_event_callbacks(csi_, &callbacks, this), "CSI callbacks") ||
      !ok(esp_cam_ctlr_enable(csi_), "CSI enable")) {
    return false;
  }
  csi_enabled_ = true;
  if (!ok(esp_cam_ctlr_start(csi_), "CSI start")) {
    return false;
  }
  csi_started_ = true;

  esp_isp_processor_cfg_t config{};
  config.clk_src = ISP_CLK_SRC_DEFAULT;
  config.clk_hz = 80000000U;
  config.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
  config.input_data_color_type = ISP_COLOR_RAW10;
  config.output_data_color_type = ISP_COLOR_RGB565;
  config.yuv_range = ISP_COLOR_RANGE_FULL;
  config.yuv_std = ISP_YUV_CONV_STD_BT601;
  config.h_res = kWidth;
  config.v_res = kHeight;
  config.bayer_order = COLOR_RAW_ELEMENT_ORDER_GBRG;
  if (!ok(esp_isp_new_processor(&config, &isp_), "ISP create") ||
      !ok(esp_isp_enable(isp_), "ISP enable")) {
    return false;
  }
  isp_enabled_ = true;
  sensor_started_ = true;
  return sensor_stream(true) && balance_white();
}

bool MipiCsiCamera::release_awb() noexcept {
  if (awb_enabled_) {
    if (!ok(esp_isp_awb_controller_disable(awb_), "AWB disable")) {
      return false;
    }
    awb_enabled_ = false;
  }
  if (awb_) {
    if (!ok(esp_isp_del_awb_controller(awb_), "AWB release")) {
      return false;
    }
    awb_ = nullptr;
  }
  return true;
}

bool MipiCsiCamera::balance_white() noexcept {
  // 스트림 시작 시 중앙 영역의 중간 밝기 화소를 한 프레임 측정한다.
  // 기준 화소가 부족하면 현재 조명에서 측정 가능한 다음 시작까지 기본 색을 사용한다.
  constexpr std::uint32_t kMinWhitePixels = 2000U;
  esp_isp_awb_config_t config{};
  config.sample_point = ISP_AWB_SAMPLE_POINT_BEFORE_CCM;
  config.window = {{144U, 84U}, {1143U, 643U}};
  config.white_patch.luminance = {60U, 540U};
  config.white_patch.red_green_ratio = {0.34F, 1.15F};
  config.white_patch.blue_green_ratio = {0.40F, 1.15F};
  if (!ok(esp_isp_new_awb_controller(isp_, &config, &awb_), "AWB create")) {
    return true;
  }
  awb_enabled_ = ok(esp_isp_awb_controller_enable(awb_), "AWB enable");
  isp_awb_stat_result_t stats{};
  const bool sampled = awb_enabled_ &&
      ok(esp_isp_awb_controller_get_oneshot_statistics(awb_, 250, &stats), "AWB sample");
  if (!release_awb()) {
    return false;
  }
  if (!sampled || stats.white_patch_num < kMinWhitePixels || !stats.sum_r || !stats.sum_g || !stats.sum_b) {
    HMI_LOG_INFO("[camera] white balance skipped samples=%u\r\n", static_cast<unsigned>(stats.white_patch_num));
    return true;
  }

  const auto green = static_cast<float>(stats.sum_g);
  esp_isp_ccm_config_t correction{};
  correction.matrix[0][0] = std::clamp(green / static_cast<float>(stats.sum_r), 0.85F, 2.75F);
  correction.matrix[1][1] = 1.0F;
  correction.matrix[2][2] = std::clamp(green / static_cast<float>(stats.sum_b), 0.85F, 2.50F);
  ccm_enabled_ = ok(esp_isp_ccm_configure(isp_, &correction), "white balance CCM configure") &&
                 ok(esp_isp_ccm_enable(isp_), "white balance CCM enable");
  HMI_LOG_INFO("[camera] white balance %s samples=%u R=%u G=100 B=%u\r\n",
               ccm_enabled_ ? "applied" : "skipped", static_cast<unsigned>(stats.white_patch_num),
               static_cast<unsigned>(correction.matrix[0][0] * 100.0F),
               static_cast<unsigned>(correction.matrix[2][2] * 100.0F));
  return true;
}

CameraStartResult MipiCsiCamera::fail(CameraStartResult result) noexcept {
  stop();
  return result;
}

CameraStartResult MipiCsiCamera::start() noexcept {
  if (!stop()) {
    return CameraStartResult::stream_failed;
  }
  // DSI가 Board 초기화에서 같은 PHY 전원을 켜므로 별도의 LDO 취득이 없다.
  std::uint8_t high = 0U;
  std::uint8_t low = 0U;
  if (!read_register(0x300aU, high) || !read_register(0x300bU, low) || high != 0x56U || low != 0x02U) {
    return CameraStartResult::probe_failed;
  }
  for (const auto& reg : kOv02c10Mode1288x728) {
    if (reg.address == 0xfffeU) {
      delay(reg.value);
    } else if (!write_register(reg.address, reg.value)) {
      return CameraStartResult::probe_failed;
    }
  }

  esp_cam_ctlr_csi_config_t config{};
  config.ctlr_id = 0;
  config.clk_src = MIPI_CSI_PHY_CLK_SRC_DEFAULT;
  config.h_res = kWidth;
  config.v_res = kHeight;
  config.data_lane_num = 1U;
  config.lane_bit_rate_mbps = 400;
  config.input_data_color_type = CAM_CTLR_COLOR_RAW10;
  config.output_data_color_type = CAM_CTLR_COLOR_RGB565;
  config.queue_items = 1;
  config.bk_buffer_dis = true;
  if (!ok(esp_cam_new_csi_ctlr(&config, &csi_), "CSI create")) {
    return fail(CameraStartResult::stream_failed);
  }
  for (auto& buffer : buffers_) {
    buffer = static_cast<std::uint8_t*>(esp_cam_ctlr_alloc_buffer(csi_, kFrameBytes, kBufferCaps));
    if (!buffer) {
      return fail(CameraStartResult::out_of_memory);
    }
  }
  if (!start_receiver()) {
    return fail(CameraStartResult::stream_failed);
  }
  portENTER_CRITICAL(&slots_mux_);
  accepting_frames_ = true;
  portEXIT_CRITICAL(&slots_mux_);
  return CameraStartResult::ready;
}

CameraFrame MipiCsiCamera::borrow_frame() noexcept {
  CameraFrame frame;
  portENTER_CRITICAL(&slots_mux_);
  if (accepting_frames_) {
    const auto loan = slots_.borrow();
    if (loan.id != 0U) {
      frame = {buffers_[loan.index], kFrameBytes, loan.id, kWidth, kHeight, kStride};
    }
  }
  portEXIT_CRITICAL(&slots_mux_);
  return frame;
}

void MipiCsiCamera::return_frame(std::uint32_t id) noexcept {
  portENTER_CRITICAL(&slots_mux_);
  slots_.release(id);
  portEXIT_CRITICAL(&slots_mux_);
}

bool MipiCsiCamera::stop() noexcept {
  portENTER_CRITICAL(&slots_mux_);
  if (slots_.borrowed()) {
    portEXIT_CRITICAL(&slots_mux_);
    return false;
  }
  accepting_frames_ = false;
  portEXIT_CRITICAL(&slots_mux_);

  if (!release_awb()) {
    return false;
  }
  if (ccm_enabled_) {
    if (!ok(esp_isp_ccm_disable(isp_), "white balance CCM disable")) {
      return false;
    }
    ccm_enabled_ = false;
  }
  if (sensor_started_ && sensor_stream(false)) {
    sensor_started_ = false;
  }
  if (csi_started_) {
    if (!ok(esp_cam_ctlr_stop(csi_), "CSI stop")) {
      return false;
    }
    csi_started_ = false;
  }
  if (csi_enabled_) {
    if (!ok(esp_cam_ctlr_disable(csi_), "CSI disable")) {
      return false;
    }
    csi_enabled_ = false;
  }
  if (csi_) {
    if (!ok(esp_cam_ctlr_del(csi_), "CSI release")) {
      return false;
    }
    csi_ = nullptr;
  }
  if (isp_enabled_ && ok(esp_isp_disable(isp_), "ISP disable")) {
    isp_enabled_ = false;
  }
  if (isp_ && !isp_enabled_ && ok(esp_isp_del_processor(isp_), "ISP release")) {
    isp_ = nullptr;
  }
  portENTER_CRITICAL(&slots_mux_);
  slots_.clear();
  portEXIT_CRITICAL(&slots_mux_);
  for (auto& buffer : buffers_) {
    heap_caps_free(buffer);
    buffer = nullptr;
  }
  return !sensor_started_ && !isp_;
}

}  // namespace hmi::boards::jc1060p470c::internal
