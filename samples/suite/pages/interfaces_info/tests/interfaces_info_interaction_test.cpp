#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <string>

#include <lvgl.h>

#include "app.hpp"
#include "pointer_test_driver.hpp"
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

void assert_article_end_is_readable(lv_obj_t* screen, const char* heading) {
  lv_obj_t* label = find_label(screen, heading);
  assert(label != nullptr);
  lv_obj_t* section = lv_obj_get_parent(label);
  assert(!lv_obj_has_flag(section, LV_OBJ_FLAG_CLICKABLE));
  assert(!lv_obj_has_flag(section, LV_OBJ_FLAG_CLICK_FOCUSABLE));
  lv_obj_t* paragraph = lv_obj_get_child(section, 1);
  lv_obj_t* scroll = lv_obj_get_parent(section);
  lv_obj_update_layout(screen);
  lv_obj_scroll_to_view_recursive(paragraph, LV_ANIM_OFF);

  lv_area_t text{};
  lv_area_t viewport{};
  lv_obj_get_coords(paragraph, &text);
  lv_obj_get_content_coords(scroll, &viewport);
  assert(text.x1 >= viewport.x1 && text.x2 <= viewport.x2);
  assert(text.y1 >= viewport.y1 && text.y2 <= viewport.y2);
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
  assert(find_label(lv_scr_act(), "사용 가능한 기능") != nullptr);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    const std::string list_path = std::string(argv[2]) + ".list.bmp";
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(list_path.c_str()));
  }

  constexpr std::array<const char*, 8U> titles{{
      "RS485 통신", "디지털 GPIO", "PWM 출력", "USB CDC 명령",
      "SD 파일 접근", "USB 대용량 저장소", "USB 자산 업데이트",
      "펌웨어 업데이트 기반",
  }};
  lv_obj_t* first_title = find_label(lv_scr_act(), titles.front());
  assert(first_title != nullptr);
  lv_obj_t* first_row = lv_obj_get_parent(lv_obj_get_parent(first_title));
  struct PressFeedback {
    lv_color_t resting_color;
    bool highlighted = false;
  } feedback{lv_obj_get_style_bg_color(first_row, LV_PART_MAIN)};
  lv_obj_add_event_cb(
      first_row,
      [](lv_event_t* event) {
        auto& feedback = *static_cast<PressFeedback*>(lv_event_get_user_data(event));
        auto* row = lv_event_get_target(event);
        feedback.highlighted = lv_obj_has_state(row, LV_STATE_PRESSED) &&
            lv_obj_get_style_bg_color(row, LV_PART_MAIN).full != feedback.resting_color.full;
      },
      LV_EVENT_PRESSED, &feedback);
  lv_obj_update_layout(lv_scr_act());
  lv_area_t row_bounds{};
  lv_obj_get_coords(first_row, &row_bounds);
  hmi::platform::windows::testing::click_pointer(
      *lvgl, row_bounds.x1 + 24, (row_bounds.y1 + row_bounds.y2) / 2);
  assert(feedback.highlighted);
  assert(find_label(lv_scr_act(), "용도") != nullptr);
  click_label(lv_scr_act(), "뒤로");

  for (const char* title : titles) {
    click_label(lv_scr_act(), title);
    assert(find_label(lv_scr_act(), title) != nullptr);
    assert(find_label(lv_scr_act(), "용도") != nullptr);
    assert(find_label(lv_scr_act(), "사용 절차") != nullptr);
    assert_article_end_is_readable(lv_scr_act(), "제품 적용 요건");
    click_label(lv_scr_act(), "뒤로");
    assert(find_label(lv_scr_act(), "RS485 통신") != nullptr);
  }

  click_label(lv_scr_act(), "펌웨어 업데이트 기반");
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    const std::string detail_path = std::string(argv[2]) + ".ko-detail.bmp";
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(detail_path.c_str()));
  }
  lv_obj_t* purpose = find_label(lv_scr_act(), "용도");
  assert(purpose != nullptr);
  lv_obj_t* first_section = lv_obj_get_parent(purpose);
  lv_obj_t* article = lv_obj_get_parent(first_section);
  lv_obj_update_layout(lv_scr_act());
  lv_area_t first_section_bounds{};
  lv_obj_get_coords(first_section, &first_section_bounds);
  assert(lv_obj_get_scroll_y(article) == 0);
  hmi::platform::windows::testing::drag_pointer(
      *lvgl, first_section_bounds.x1 + 24, first_section_bounds.y2 - 10,
      first_section_bounds.y1 - 54);
  assert(lv_obj_get_scroll_y(article) > 0);
  click_label(lv_scr_act(), "영어");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Available Features") != nullptr);
  assert(find_label(lv_scr_act(), "Firmware Update Foundation") != nullptr);
  assert(find_label(lv_scr_act(), "What it provides") != nullptr);
  click_label(lv_scr_act(), "Dark");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Firmware Update Foundation") != nullptr);
  assert(find_label(lv_scr_act(), "Light") != nullptr);

  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(argv[2]));
  }
  assert_article_end_is_readable(lv_scr_act(), "Product requirements");

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
