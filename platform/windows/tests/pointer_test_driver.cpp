#include "pointer_test_driver.hpp"

#include <cassert>

#include <SDL.h>

namespace hmi::platform::windows::testing {
namespace {

void push_button(std::uint32_t type, int x, int y) {
  SDL_Event event{};
  event.type = type;
  event.button.button = SDL_BUTTON_LEFT;
  event.button.x = x;
  event.button.y = y;
  assert(SDL_PushEvent(&event) == 1);
}

}  // namespace

void click_pointer(SdlLvglPort &port, int x, int y) {
  SDL_Event event{};
  event.type = SDL_MOUSEMOTION;
  event.motion.x = x;
  event.motion.y = y;
  assert(SDL_PushEvent(&event) == 1);
  push_button(SDL_MOUSEBUTTONDOWN, x, y);
  assert(port.process_events());
  SDL_Delay(40U);
  port.tick();
  push_button(SDL_MOUSEBUTTONUP, x, y);
  assert(port.process_events());
  SDL_Delay(40U);
  port.tick();
}

void hold_pointer(SdlLvglPort &port, int x, int y) {
  push_button(SDL_MOUSEBUTTONDOWN, x, y);
  assert(port.process_events());
  for (int step = 0; step < 12; ++step) {
    SDL_Delay(50U);
    port.tick();
  }
  push_button(SDL_MOUSEBUTTONUP, x, y);
  assert(port.process_events());
  SDL_Delay(40U);
  port.tick();
}

void drag_pointer(SdlLvglPort &port, int x, int from_y, int to_y) {
  push_button(SDL_MOUSEBUTTONDOWN, x, from_y);
  assert(port.process_events());
  SDL_Delay(30U);
  port.tick();
  for (int step = 1; step <= 6; ++step) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.state = SDL_BUTTON_LMASK;
    event.motion.x = x;
    event.motion.y = from_y + ((to_y - from_y) * step / 6);
    assert(SDL_PushEvent(&event) == 1);
    assert(port.process_events());
    SDL_Delay(30U);
    port.tick();
  }
  push_button(SDL_MOUSEBUTTONUP, x, to_y);
  assert(port.process_events());
  SDL_Delay(40U);
  port.tick();
}

}  // namespace hmi::platform::windows::testing
