#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <lvgl.h>

#include "internal/mipi_dsi_display.hpp"
#include "internal/touch_controller.hpp"

namespace hmi::boards::jc1060p470c::internal {

/**
 * Board의 display와 touch를 LVGL 8.4 driver API에 연결한다.
 *
 * ESP LCD가 소유한 두 framebuffer를 빌려 full-refresh draw buffer로 사용한다.
 * 앱 화면은 만들지 않으며 Board가 이 객체의 lifetime과 tick 호출을 소유한다.
 */
class LvglPort {
 public:
  bool attach_display(MipiDsiDisplay &display);
  bool attach_touch(TouchController &touch);
  void tick(std::uint32_t now_ms);

 private:
  static void display_flush(lv_disp_drv_t *driver, const lv_area_t *area,
                            lv_color_t *colors);
  static void touch_read(lv_indev_drv_t *driver, lv_indev_data_t *data);

  MipiDsiDisplay *display_ = nullptr;
  TouchController *touch_ = nullptr;
  lv_disp_draw_buf_t draw_buffer_{};
  lv_disp_drv_t display_driver_{};
  lv_indev_drv_t touch_driver_{};
  lv_disp_t *display_handle_ = nullptr;
  lv_indev_t *touch_handle_ = nullptr;
  SemaphoreHandle_t refresh_semaphore_ = nullptr;
  std::uint32_t last_tick_ms_ = 0;
};

}  // namespace hmi::boards::jc1060p470c::internal
