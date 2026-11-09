#pragma once

#include <array>
#include <cstdint>

#include <SDL.h>
#include <lvgl.h>

namespace hmi::platform::windows {

enum class KeyboardCommand : std::uint8_t {
  backspace,
  enter,
  cursor_left,
  cursor_right,
};

struct KeyboardCallbacks {
  void *context = nullptr;
  void (*text)(void *context, const char *utf8) noexcept = nullptr;
  void (*command)(void *context, KeyboardCommand command) noexcept = nullptr;
};

/**
 * SDL 창·mouse 입력·LVGL draw buffer를 소유하는 Windows display port다.
 *
 * 앱 화면은 소유하지 않는다. host의 단일 UI thread가 begin, process_events,
 * tick을 호출하며 host는 앱을 먼저 정리한 뒤 이 객체를 파괴해야 한다. LVGL 8의
 * 전역 display state는 process lifetime이며 port 파괴 뒤 LVGL API를 다시 호출하지 않는다.
 */
class SdlLvglPort {
 public:
  static constexpr int kWidth = 1024;
  static constexpr int kHeight = 600;
  static constexpr int kBufferLines = 40;
  static constexpr std::size_t kBufferPixels =
      static_cast<std::size_t>(kWidth) * kBufferLines;
  static constexpr std::size_t kBufferBytes =
      kBufferPixels * sizeof(lv_color_t);

  SdlLvglPort() = default;
  SdlLvglPort(const SdlLvglPort &) = delete;
  SdlLvglPort &operator=(const SdlLvglPort &) = delete;

  bool begin(const char *window_title, bool visible = true);
  void set_keyboard_callbacks(KeyboardCallbacks callbacks) noexcept {
    keyboard_callbacks_ = callbacks;
  }
  bool process_events();
  void tick();
  /** OS 밝기를 건드리지 않고 simulator 출력만 1~10 단계로 감광한다. */
  bool set_brightness_level(std::uint8_t level) noexcept;
  [[nodiscard]] std::uint8_t brightness_level() const noexcept {
    return brightness_level_;
  }
  bool save_screenshot(const char *path);
  ~SdlLvglPort();

 private:
  static void flush(lv_disp_drv_t *driver, const lv_area_t *area,
                    lv_color_t *pixels);
  static void read_mouse(lv_indev_drv_t *driver, lv_indev_data_t *data);
  void shutdown() noexcept;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;
  lv_disp_t *display_ = nullptr;
  lv_indev_t *mouse_ = nullptr;
  lv_disp_draw_buf_t draw_buffer_{};
  lv_disp_drv_t display_driver_{};
  lv_indev_drv_t mouse_driver_{};
  std::array<lv_color_t, kBufferPixels> pixels_{};
  std::int16_t mouse_x_ = 0;
  std::int16_t mouse_y_ = 0;
  bool mouse_down_ = false;
  bool sdl_initialized_ = false;
  std::uint8_t brightness_level_ = 10U;
  std::uint32_t last_tick_ms_ = 0U;
  KeyboardCallbacks keyboard_callbacks_{};
};

static_assert(sizeof(lv_color_t) == 2U,
              "Windows draw buffer must use RGB565");
static_assert(SdlLvglPort::kBufferBytes == 81920U,
              "Windows draw buffer size changed unexpectedly");

}  // namespace hmi::platform::windows
