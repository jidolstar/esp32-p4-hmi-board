#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <string>

#include <lvgl.h>

#include "app.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

lv_obj_t* find_label(lv_obj_t* parent, const char* text) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(parent, &lv_label_class) &&
      std::strcmp(lv_label_get_text(parent), text) == 0) {
    return parent;
  }
  const auto count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0U; index < count; ++index) {
    if (lv_obj_t* found = find_label(lv_obj_get_child(parent, index), text)) {
      return found;
    }
  }
  return nullptr;
}

void click_label(lv_obj_t* screen, const char* text) {
  lv_obj_t* target = find_label(screen, text);
  assert(target != nullptr);
  while (target != nullptr &&
         !lv_obj_has_flag(target, LV_OBJ_FLAG_CLICKABLE)) {
    target = lv_obj_get_parent(target);
  }
  assert(target != nullptr);
  const auto result = lv_event_send(target, LV_EVENT_SHORT_CLICKED, nullptr);
  assert(result == LV_RES_OK || result == LV_RES_INV);
}

}  // namespace

int main(int argc, char** argv) {
  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("interfaces info interaction", false));
  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  hmi::profiles::AppContext context{"windows"};
  assert(app->create(context));
  assert(app->request_page("interfaces-info"));
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "인터페이스와 업데이트") != nullptr);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    const std::string list_path = std::string(argv[2]) + ".list.bmp";
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(list_path.c_str()));
  }

  constexpr std::array<const char*, 8U> titles{{
      "RS485 통신", "디지털 GPIO", "PWM 출력", "USB CDC 명령",
      "저장소 파일 접근", "USB 대용량 저장소", "USB 자산 업데이트",
      "펌웨어 업데이트 기반",
  }};
  for (const char* title : titles) {
    click_label(lv_scr_act(), title);
    assert(find_label(lv_scr_act(), title) != nullptr);
    assert(find_label(lv_scr_act(), "언제 사용하는가") != nullptr);
    assert(find_label(lv_scr_act(), "이번 범위에서 제외") != nullptr);
    click_label(lv_scr_act(), "뒤로");
    assert(find_label(lv_scr_act(), "RS485 통신") != nullptr);
  }

  click_label(lv_scr_act(), "펌웨어 업데이트 기반");
  click_label(lv_scr_act(), "영어");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Firmware Update Foundation") != nullptr);
  assert(find_label(lv_scr_act(), "Provided contract") != nullptr);
  click_label(lv_scr_act(), "Dark");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Firmware Update Foundation") != nullptr);
  assert(find_label(lv_scr_act(), "Light") != nullptr);

  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(argv[2]));
  }

  click_label(lv_scr_act(), "Back");
  assert(find_label(lv_scr_act(), "RS485 Communication") != nullptr);
  click_label(lv_scr_act(), "Back");
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == hmi::samples::suite::SuiteView::kDashboard);

  for (unsigned iteration = 0U; iteration < 10U; ++iteration) {
    assert(app->request_page("interfaces-info"));
    assert(app->flush_pending_transition());
    assert(app->request_dashboard());
    assert(app->flush_pending_transition());
  }
  app->destroy();
  return 0;
}
