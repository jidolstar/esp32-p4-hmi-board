#include "sdl_lvgl_port.hpp"

#include <SDL.h>
#include <lvgl.h>

#include <cassert>
#include <cstring>
#include <memory>

namespace {

struct KeyboardState {
  unsigned text_events = 0U;
  unsigned command_events = 0U;
  char text[32]{};
  hmi::platform::windows::KeyboardCommand command =
      hmi::platform::windows::KeyboardCommand::enter;
};

void handle_text(void *context, const char *utf8) noexcept {
  auto *state = static_cast<KeyboardState *>(context);
  ++state->text_events;
  std::strncpy(state->text, utf8, sizeof(state->text) - 1U);
}

void handle_command(
    void *context,
    hmi::platform::windows::KeyboardCommand command) noexcept {
  auto *state = static_cast<KeyboardState *>(context);
  ++state->command_events;
  state->command = command;
}

}  // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  lv_init();

  // draw buffer가 큰 port는 host와 같은 방식으로 heap에서 소유한다.
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(port->begin("HMI SDL event test", false));

  if (std::strcmp(argv[1], "keyboard") == 0) {
    KeyboardState state{};
    port->set_keyboard_callbacks({&state, &handle_text, &handle_command});
    SDL_Event text_event{};
    text_event.type = SDL_TEXTINPUT;
    std::strcpy(text_event.text.text, "한A");
    assert(SDL_PushEvent(&text_event) == 1);
    SDL_Event key_event{};
    key_event.type = SDL_KEYDOWN;
    key_event.key.repeat = 0;
    key_event.key.keysym.sym = SDLK_BACKSPACE;
    assert(SDL_PushEvent(&key_event) == 1);
    assert(port->process_events());
    assert(state.text_events == 1U);
    assert(std::strcmp(state.text, "한A") == 0);
    assert(state.command_events == 1U);
    assert(state.command == hmi::platform::windows::KeyboardCommand::backspace);
    return 0;
  }

  SDL_Event event{};
  if (std::strcmp(argv[1], "escape") == 0) {
    event.type = SDL_KEYDOWN;
    event.key.repeat = 0;
    event.key.keysym.sym = SDLK_ESCAPE;
  } else {
    assert(std::strcmp(argv[1], "quit") == 0);
    event.type = SDL_QUIT;
  }
  assert(SDL_PushEvent(&event) == 1);
  assert(!port->process_events());
  return 0;
}
