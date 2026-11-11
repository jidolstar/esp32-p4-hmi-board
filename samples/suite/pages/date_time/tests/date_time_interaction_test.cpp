#include <cassert>
#include <cstring>
#include <memory>

#include <lvgl.h>

#include "app.hpp"
#include "sdl_lvgl_port.hpp"
#include "time/virtual_time_sync_ports.hpp"

namespace {

lv_obj_t* find_label(lv_obj_t* parent, const char* text) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(parent, &lv_label_class) &&
      std::strcmp(lv_label_get_text(parent), text) == 0) {
    return parent;
  }
  const std::uint32_t count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0U; index < count; ++index) {
    if (lv_obj_t* result = find_label(lv_obj_get_child(parent, index), text)) {
      return result;
    }
  }
  return nullptr;
}

void click_label(lv_obj_t* screen, const char* text) {
  lv_obj_t* label = find_label(screen, text);
  assert(label != nullptr);
  lv_obj_t* button = lv_obj_get_parent(label);
  assert(button != nullptr);
  const lv_res_t result =
      lv_event_send(button, LV_EVENT_SHORT_CLICKED, nullptr);
  assert(result == LV_RES_OK || result == LV_RES_INV);
}

}  // namespace

int main(int argc, char** argv) {
  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("date-time interaction", false));

  hmi::platform::windows::time::VirtualTimeSyncPorts virtual_time;
  virtual_time.set_rtc({2026U, 11U, 11U, 10U, 30U, 0U},
                       hmi::time::RtcStatus::not_detected);
  hmi::time::DateTimeModel model;
  hmi::time::TimeSyncController controller;
  model.begin(0U);
  auto ports = virtual_time.ports();
  ports.update = hmi::time::date_time_model_sink(model);
  assert(controller.begin(ports, 0U));
  assert(model.apply({2026U, 11U, 11U, 10U, 30U, 0U},
                     hmi::time::TimeSource::manual, 0U));
  hmi::time::TimeRuntime runtime{&model, &controller};

  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  hmi::profiles::AppContext context;
  context.host_name = "windows";
  context.time_runtime = &runtime;
  assert(app->create(context));
  assert(app->request_page("date-time"));
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "날짜와 시간") != nullptr);
  assert(find_label(lv_scr_act(), "2026-11-11") != nullptr);
  assert(find_label(lv_scr_act(), "수동 설정") != nullptr);
  assert(find_label(lv_scr_act(), "초") != nullptr);
  assert(find_label(lv_scr_act(),
                    "RTC 칩 확인 전 · 현재 사용 안 함") != nullptr);
  lv_obj_update_layout(lv_scr_act());

  lv_obj_t* current_group =
      lv_obj_get_parent(find_label(lv_scr_act(), "현재 시각"));
  lv_obj_t* source_group =
      lv_obj_get_parent(find_label(lv_scr_act(), "시간 출처"));
  lv_obj_t* rtc_group = lv_obj_get_parent(find_label(lv_scr_act(), "RTC 상태"));
  lv_obj_t* network_group =
      lv_obj_get_parent(find_label(lv_scr_act(), "네트워크 동기화"));
  assert(current_group != nullptr && source_group != nullptr &&
         rtc_group != nullptr && network_group != nullptr);
  assert(current_group != source_group && source_group != rtc_group &&
         rtc_group != network_group);
  for (lv_obj_t* group :
       {current_group, source_group, rtc_group, network_group}) {
    assert(lv_obj_get_style_border_width(group, LV_PART_MAIN) > 0);
  }

  lv_obj_t* footer_label = find_label(
      lv_scr_act(), "수동 설정은 시스템 시각에 먼저 적용됩니다.");
  assert(footer_label != nullptr);
  lv_obj_t* footer = lv_obj_get_parent(footer_label);
  assert(footer != nullptr);
  lv_area_t label_area{};
  lv_area_t footer_area{};
  lv_obj_get_coords(footer_label, &label_area);
  lv_obj_get_coords(footer, &footer_area);
  const int left_inset = label_area.x1 - footer_area.x1;
  const int center_delta = (label_area.y1 + label_area.y2) -
                           (footer_area.y1 + footer_area.y2);
  assert(left_inset >= 36 && left_inset <= 44);
  assert(center_delta >= -2 && center_delta <= 2);
  assert(lv_obj_get_style_text_align(footer_label, LV_PART_MAIN) ==
         LV_TEXT_ALIGN_LEFT);

  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(argv[2]));
  }

  click_label(lv_scr_act(), "날짜·시각 적용");
  assert(find_label(lv_scr_act(),
                    "시스템 시각과 RTC에 적용했습니다.") != nullptr);
  click_label(lv_scr_act(), "영어");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Date & Time") != nullptr);
  assert(find_label(lv_scr_act(), "Manual setting") != nullptr);
  assert(find_label(lv_scr_act(), "Second") != nullptr);
  click_label(lv_scr_act(), "Dark");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Light") != nullptr);
  click_label(lv_scr_act(), "Back");
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == hmi::samples::suite::SuiteView::kDashboard);
  app->destroy();
  return 0;
}
