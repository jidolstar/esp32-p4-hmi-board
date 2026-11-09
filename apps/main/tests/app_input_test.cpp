#include <lvgl.h>

#include <cstdint>
#include <cstring>
#include <memory>

#include "app.hpp"
#include "sdl_lvgl_port.hpp"

int main() {
  lv_init();
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  if (!port->begin("HMI Main Input Test", false)) {
    return 1;
  }

  hmi::apps::main::MainApp app;
  hmi::profiles::AppContext context;
  context.host_name = "windows";
  if (!app.create(context)) {
    return 2;
  }

  lv_obj_t *panel = lv_obj_get_child(lv_scr_act(), 0);
  if (panel == nullptr) {
    return 3;
  }
  lv_event_send(panel, LV_EVENT_PRESSED, nullptr);

  const std::uint32_t child_count = lv_obj_get_child_cnt(panel);
  if (child_count == 0) {
    return 4;
  }
  lv_obj_t *input_status = lv_obj_get_child(panel, child_count - 1);
  if (input_status == nullptr ||
      std::strcmp(lv_label_get_text(input_status), "Input: detected") != 0) {
    return 5;
  }

  port->tick();
  return 0;
}
