#pragma once

#include <cstdint>

#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>

namespace hmi::boards::jc1060p470c::internal {

/**
 * JC1060P470C-I-W-Y의 JD9165 MIPI-DSI panel과 framebuffer lifetime을 소유한다.
 *
 * ESP LCD driver가 PSRAM에 할당한 RGB565 framebuffer 두 장만 사용한다. 별도
 * frame array를 만들지 않으며 UI 객체 생성은 이 객체의 책임이 아니다.
 */
class MipiDsiDisplay {
 public:
  bool begin();
  void set_backlight(bool on);
  /** 밝기 단계 1~10을 체감 보정된 PWM duty로 적용한다. */
  bool set_backlight_level(std::uint8_t level);

  esp_lcd_panel_handle_t panel_handle() const;
  void *framebuffer_a() const;
  void *framebuffer_b() const;

 private:
  esp_ldo_channel_handle_t phy_power_ = nullptr;
  esp_lcd_dsi_bus_handle_t dsi_bus_ = nullptr;
  esp_lcd_panel_io_handle_t command_io_ = nullptr;
  esp_lcd_panel_handle_t panel_ = nullptr;
  void *framebuffer_a_ = nullptr;
  void *framebuffer_b_ = nullptr;
  bool backlight_pwm_attached_ = false;
  std::uint8_t backlight_duty_ = 255U;
};

}  // namespace hmi::boards::jc1060p470c::internal
