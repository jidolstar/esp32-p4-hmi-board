#pragma once

#include "sdl_lvgl_port.hpp"

namespace hmi::platform::windows::testing {

// Windows interaction test에서 실제 pointer read callback 경로를 구동한다.
void click_pointer(SdlLvglPort &port, int x, int y);
void hold_pointer(SdlLvglPort &port, int x, int y);
void drag_pointer(SdlLvglPort &port, int x, int from_y, int to_y);

}  // namespace hmi::platform::windows::testing
