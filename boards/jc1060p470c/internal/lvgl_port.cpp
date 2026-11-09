#include "internal/lvgl_port.hpp"

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_lcd_panel_ops.h>

#include "config.hpp"
#include "log.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

static_assert(sizeof(lv_color_t) == 2,
              "JC1060P470C LVGL port requires RGB565 colors");

// 현재 firmware는 display 한 개만 지원하며 callback은 Board lifetime과 함께 유지된다.
LvglPort *active_port = nullptr;

// Flash cache가 꺼진 ISR에서도 semaphore storage가 internal RAM에 남아 있어야 한다.
DRAM_ATTR StaticSemaphore_t refresh_semaphore_buffer;

bool IRAM_ATTR refresh_done(esp_lcd_panel_handle_t,
                            esp_lcd_dpi_panel_event_data_t *, void *context) {
  BaseType_t awakened = pdFALSE;
  xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(context), &awakened);
  return awakened == pdTRUE;
}

}  // namespace

bool LvglPort::attach_display(MipiDsiDisplay &display) {
  display_ = &display;
  active_port = this;

  auto *buffer_a = static_cast<lv_color_t *>(display.framebuffer_a());
  auto *buffer_b = static_cast<lv_color_t *>(display.framebuffer_b());
  if (buffer_a == nullptr || buffer_b == nullptr) {
    HMI_LOG_ERROR("[lvgl] framebuffer is not ready\r\n");
    return false;
  }

  constexpr std::size_t kBufferPixels =
      static_cast<std::size_t>(config::kScreenWidth) * config::kScreenHeight;
  lv_disp_draw_buf_init(&draw_buffer_, buffer_a, buffer_b, kBufferPixels);

  lv_disp_drv_init(&display_driver_);
  display_driver_.hor_res = config::kScreenWidth;
  display_driver_.ver_res = config::kScreenHeight;
  display_driver_.flush_cb = &LvglPort::display_flush;
  display_driver_.draw_buf = &draw_buffer_;
  display_driver_.full_refresh = 1;

  refresh_semaphore_ = xSemaphoreCreateBinaryStatic(&refresh_semaphore_buffer);
  if (refresh_semaphore_ == nullptr) {
    HMI_LOG_ERROR("[lvgl] refresh semaphore unavailable\r\n");
    return false;
  }

  const esp_lcd_dpi_panel_event_callbacks_t callbacks = {
      .on_color_trans_done = nullptr,
      .on_refresh_done = refresh_done,
  };
  if (esp_lcd_dpi_panel_register_event_callbacks(
          display.panel_handle(), &callbacks, refresh_semaphore_) != ESP_OK) {
    HMI_LOG_ERROR("[lvgl] refresh callback registration failed\r\n");
    return false;
  }

  display_handle_ = lv_disp_drv_register(&display_driver_);
  HMI_LOG_INFO("[lvgl] display registered pixels=%u\r\n",
               static_cast<unsigned>(kBufferPixels));
  return display_handle_ != nullptr;
}

bool LvglPort::attach_touch(TouchController &touch) {
  touch_ = &touch;
  active_port = this;

  lv_indev_drv_init(&touch_driver_);
  touch_driver_.type = LV_INDEV_TYPE_POINTER;
  touch_driver_.read_cb = &LvglPort::touch_read;
  touch_handle_ = lv_indev_drv_register(&touch_driver_);
  HMI_LOG_INFO("[lvgl] touch registered=%s\r\n",
               touch_handle_ != nullptr ? "yes" : "no");
  return touch_handle_ != nullptr;
}

void LvglPort::tick(std::uint32_t now_ms) {
  if (last_tick_ms_ == 0) {
    last_tick_ms_ = now_ms;
  }
  lv_tick_inc(now_ms - last_tick_ms_);
  last_tick_ms_ = now_ms;
  lv_timer_handler();
}

void LvglPort::display_flush(lv_disp_drv_t *driver, const lv_area_t *,
                             lv_color_t *colors) {
  LvglPort *self = active_port;
  if (self != nullptr && self->display_ != nullptr &&
      self->display_->panel_handle() != nullptr &&
      lv_disp_flush_is_last(driver)) {
    // 이전 refresh 신호를 지운 뒤 새 frame을 제출해 buffer 재사용 시점을 구분한다.
    xSemaphoreTake(self->refresh_semaphore_, 0);
    const esp_err_t result = esp_lcd_panel_draw_bitmap(
        self->display_->panel_handle(), 0, 0, config::kScreenWidth,
        config::kScreenHeight, colors);
    if (result == ESP_OK) {
      if (xSemaphoreTake(self->refresh_semaphore_, pdMS_TO_TICKS(100)) !=
          pdTRUE) {
        HMI_LOG_ERROR("[lvgl] display refresh timeout\r\n");
      }
    } else {
      HMI_LOG_ERROR("[lvgl] display draw failed err=0x%x\r\n", result);
    }
  }
  lv_disp_flush_ready(driver);
}

void LvglPort::touch_read(lv_indev_drv_t *, lv_indev_data_t *data) {
  TouchPoint point;
  if (active_port != nullptr && active_port->touch_ != nullptr) {
    point = active_port->touch_->read();
  }

  data->state = point.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  data->point.x = point.x;
  data->point.y = point.y;
}

}  // namespace hmi::boards::jc1060p470c::internal
