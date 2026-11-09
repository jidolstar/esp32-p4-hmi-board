#include "sdl_lvgl_port.hpp"

#include <algorithm>
#include <cstdio>

namespace hmi::platform::windows {
namespace {

// 실제 LCD PWM과 달리 RGB modulation은 선형 감광이므로 최저 단계도 조작 가능한
// 밝기를 유지하면서 단계 차이가 눈에 보이도록 별도 simulator table을 사용한다.
constexpr std::uint8_t kBrightnessModulation[] = {
    80U, 96U, 112U, 128U, 148U, 168U, 188U, 208U, 232U, 255U,
};

}  // namespace

bool SdlLvglPort::begin(const char *window_title, bool visible) {
  if (window_title == nullptr || window_title[0] == '\0' || sdl_initialized_) {
    return false;
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }
  sdl_initialized_ = true;

  const std::uint32_t window_flags = visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN;
  window_ = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, kWidth, kHeight,
                             window_flags);
  if (window_ != nullptr) {
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (renderer_ == nullptr) {
      renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
  }
  if (renderer_ != nullptr) {
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565,
                                 SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
  }
  if (texture_ == nullptr) {
    std::fprintf(stderr, "SDL display: %s\n", SDL_GetError());
    shutdown();
    return false;
  }

  lv_disp_draw_buf_init(&draw_buffer_, pixels_.data(), nullptr, pixels_.size());
  lv_disp_drv_init(&display_driver_);
  display_driver_.hor_res = kWidth;
  display_driver_.ver_res = kHeight;
  display_driver_.draw_buf = &draw_buffer_;
  display_driver_.flush_cb = flush;
  display_driver_.user_data = this;
  display_ = lv_disp_drv_register(&display_driver_);
  if (display_ == nullptr) {
    shutdown();
    return false;
  }

  lv_indev_drv_init(&mouse_driver_);
  mouse_driver_.type = LV_INDEV_TYPE_POINTER;
  mouse_driver_.read_cb = read_mouse;
  mouse_driver_.user_data = this;
  mouse_ = lv_indev_drv_register(&mouse_driver_);
  if (mouse_ == nullptr) {
    shutdown();
    return false;
  }

  last_tick_ms_ = SDL_GetTicks();
  SDL_StartTextInput();
  return true;
}

bool SdlLvglPort::process_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return false;
    }
    if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
        event.key.keysym.sym == SDLK_ESCAPE) {
      return false;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
      mouse_down_ = false;
    } else if (event.type == SDL_MOUSEMOTION) {
      mouse_x_ = static_cast<std::int16_t>(
          std::clamp(event.motion.x, 0, kWidth - 1));
      mouse_y_ = static_cast<std::int16_t>(
          std::clamp(event.motion.y, 0, kHeight - 1));
    } else if ((event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_MOUSEBUTTONUP) &&
               event.button.button == SDL_BUTTON_LEFT) {
      mouse_x_ = static_cast<std::int16_t>(
          std::clamp(event.button.x, 0, kWidth - 1));
      mouse_y_ = static_cast<std::int16_t>(
          std::clamp(event.button.y, 0, kHeight - 1));
      mouse_down_ = event.type == SDL_MOUSEBUTTONDOWN;
    } else if (event.type == SDL_TEXTINPUT && keyboard_callbacks_.text != nullptr) {
      keyboard_callbacks_.text(keyboard_callbacks_.context, event.text.text);
    } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
               keyboard_callbacks_.command != nullptr) {
      KeyboardCommand command{};
      bool handled = true;
      switch (event.key.keysym.sym) {
        case SDLK_BACKSPACE:
          command = KeyboardCommand::backspace;
          break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
          command = KeyboardCommand::enter;
          break;
        case SDLK_LEFT:
          command = KeyboardCommand::cursor_left;
          break;
        case SDLK_RIGHT:
          command = KeyboardCommand::cursor_right;
          break;
        default:
          handled = false;
          break;
      }
      if (handled) {
        keyboard_callbacks_.command(keyboard_callbacks_.context, command);
      }
    }
  }
  return true;
}

void SdlLvglPort::tick() {
  const std::uint32_t now = SDL_GetTicks();
  lv_tick_inc(now - last_tick_ms_);
  last_tick_ms_ = now;
  lv_timer_handler();
  SDL_Delay(5U);
}

bool SdlLvglPort::set_brightness_level(std::uint8_t level) noexcept {
  if (texture_ == nullptr || renderer_ == nullptr || level < 1U || level > 10U) {
    return false;
  }
  const std::uint8_t modulation = kBrightnessModulation[level - 1U];
  if (SDL_SetTextureColorMod(texture_, modulation, modulation, modulation) !=
      0) {
    return false;
  }
  brightness_level_ = level;
  // 다음 LVGL invalidation을 기다리지 않아도 조절 결과가 즉시 보이게 한다.
  return SDL_RenderClear(renderer_) == 0 &&
         SDL_RenderCopy(renderer_, texture_, nullptr, nullptr) == 0 &&
         (SDL_RenderPresent(renderer_), true);
}

bool SdlLvglPort::save_screenshot(const char *path) {
  if (path == nullptr || renderer_ == nullptr || texture_ == nullptr) {
    return false;
  }
  SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(
      0, kWidth, kHeight, 32, SDL_PIXELFORMAT_ARGB8888);
  if (image == nullptr) {
    return false;
  }

  // Present 이후 backbuffer는 보장되지 않으므로 보관한 LVGL texture를 다시 그린다.
  const bool saved =
      SDL_RenderClear(renderer_) == 0 &&
      SDL_RenderCopy(renderer_, texture_, nullptr, nullptr) == 0 &&
      SDL_RenderReadPixels(renderer_, nullptr, image->format->format,
                           image->pixels, image->pitch) == 0 &&
      SDL_SaveBMP(image, path) == 0;
  SDL_FreeSurface(image);
  SDL_RenderPresent(renderer_);
  return saved;
}

void SdlLvglPort::flush(lv_disp_drv_t *driver, const lv_area_t *area,
                        lv_color_t *pixels) {
  auto *self = static_cast<SdlLvglPort *>(driver->user_data);
  SDL_Rect rectangle{area->x1, area->y1, area->x2 - area->x1 + 1,
                     area->y2 - area->y1 + 1};
  SDL_UpdateTexture(self->texture_, &rectangle, pixels,
                    rectangle.w * sizeof(lv_color_t));
  SDL_RenderCopy(self->renderer_, self->texture_, nullptr, nullptr);
  SDL_RenderPresent(self->renderer_);
  lv_disp_flush_ready(driver);
}

void SdlLvglPort::read_mouse(lv_indev_drv_t *driver, lv_indev_data_t *data) {
  auto *self = static_cast<SdlLvglPort *>(driver->user_data);
  data->point.x = self->mouse_x_;
  data->point.y = self->mouse_y_;
  data->state = self->mouse_down_ ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

void SdlLvglPort::shutdown() noexcept {
  if (mouse_ != nullptr) {
    lv_indev_delete(mouse_);
    mouse_ = nullptr;
  }
  // LVGL 8은 전역 deinit을 제공하지 않고 마지막 display 제거 시 정상 screen도
  // active-screen 삭제 경고로 기록한다. host process 종료 뒤 OS가 전역 heap을 회수한다.
  display_ = nullptr;
  if (texture_ != nullptr) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  if (renderer_ != nullptr) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  if (sdl_initialized_) {
    SDL_StopTextInput();
    SDL_Quit();
    sdl_initialized_ = false;
  }
  keyboard_callbacks_ = {};
}

SdlLvglPort::~SdlLvglPort() { shutdown(); }

}  // namespace hmi::platform::windows
