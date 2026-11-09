#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <lvgl.h>

#include "app.hpp"
#include "pointer_test_driver.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

lv_obj_t *find_label(lv_obj_t *parent, const char *text) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(parent, &lv_label_class) &&
      std::strcmp(lv_label_get_text(parent), text) == 0) {
    return parent;
  }
  const std::uint32_t count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0; index < count; ++index) {
    if (lv_obj_t *found = find_label(lv_obj_get_child(parent, index), text)) {
      return found;
    }
  }
  return nullptr;
}

void send_to_labeled_button(lv_obj_t *screen, const char *text,
                            lv_event_code_t code) {
  lv_obj_t *label = find_label(screen, text);
  assert(label != nullptr);
  lv_obj_t *button = lv_obj_get_parent(label);
  assert(button != nullptr);
  // Cancel/Back처럼 callback이 event target을 포함한 subtree를 지우면 LVGL은
  // 정상 정리 뒤 LV_RES_INV를 반환할 수 있다.
  const lv_res_t result = lv_event_send(button, code, nullptr);
  assert(result == LV_RES_OK || result == LV_RES_INV);
}

void assert_header_button_style(lv_obj_t *button) {
  assert(button != nullptr);
  assert(lv_obj_get_height(button) == 42);
  assert(lv_obj_get_style_radius(button, LV_PART_MAIN) == 8);
  assert(lv_obj_get_style_border_width(button, LV_PART_MAIN) == 1);
  assert(lv_obj_get_style_shadow_width(button, LV_PART_MAIN) == 0);
}

void assert_dialog_uses_full_screen_geometry(const char *title_text) {
  lv_obj_t *title = find_label(lv_scr_act(), title_text);
  assert(title != nullptr);
  lv_obj_t *dialog = lv_obj_get_parent(title);
  lv_obj_t *scrim = dialog == nullptr ? nullptr : lv_obj_get_parent(dialog);
  lv_obj_t *content = scrim == nullptr ? nullptr : lv_obj_get_parent(scrim);
  assert(dialog != nullptr && scrim != nullptr && content != nullptr);

  // dialog 생성 직후 좌표를 검사하므로 비동기 refresh를 기다리지 않고 현재
  // object tree의 layout을 확정한다.
  lv_obj_update_layout(content);
  assert(lv_obj_get_width(dialog) == 560);
  assert(lv_obj_get_height(dialog) == 260);

  lv_area_t content_area{};
  lv_area_t scrim_area{};
  lv_area_t dialog_area{};
  lv_obj_get_coords(content, &content_area);
  lv_obj_get_coords(scrim, &scrim_area);
  lv_obj_get_coords(dialog, &dialog_area);

  // dim layer가 화면 전체를 덮고 dialog가 그 중앙에 있어야 한다.
  assert(scrim_area.x1 == content_area.x1);
  assert(scrim_area.y1 == content_area.y1);
  assert(scrim_area.x2 == content_area.x2);
  assert(scrim_area.y2 == content_area.y2);
  assert(dialog_area.x1 + dialog_area.x2 ==
         content_area.x1 + content_area.x2);
  assert(dialog_area.y1 + dialog_area.y2 ==
         content_area.y1 + content_area.y2);
}

}  // namespace

int main(int argc, char **argv) {
  using hmi::samples::suite::OverlayState;
  using hmi::samples::suite::PageAction;
  using hmi::samples::suite::SuiteView;
  using hmi::i18n::Locale;
  namespace pointer = hmi::platform::windows::testing;

  lv_init();
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(port->begin("sample-suite interaction", argc > 1));
  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  hmi::profiles::AppContext context{"windows"};
  assert(app->create(context));

  for (int frame = 0; frame < 10; ++frame) {
    port->tick();
  }
  lv_obj_t *ui_basics_title = find_label(lv_scr_act(), "UI 기본");
  lv_obj_t *text_input_title = find_label(lv_scr_act(), "텍스트 입력");
  assert(ui_basics_title != nullptr);
  assert(text_input_title != nullptr);
  assert(find_label(lv_scr_act(), "열기") == nullptr);
  assert(find_label(lv_scr_act(), "화면 언어") == nullptr);
  assert(app->snapshot().locale == Locale::ko_kr);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    const std::string ko_dashboard_path =
        std::string(argv[2]) + ".ko-dashboard.bmp";
    assert(port->save_screenshot(ko_dashboard_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "영어", LV_EVENT_SHORT_CLICKED);
  assert(app->flush_pending_transition());
  assert(app->snapshot().locale == Locale::en_us);
  for (int frame = 0; frame < 10; ++frame) {
    port->tick();
  }
  ui_basics_title = find_label(lv_scr_act(), "UI Basics");
  text_input_title = find_label(lv_scr_act(), "Text Input");
  assert(ui_basics_title != nullptr);
  assert(text_input_title != nullptr);
  assert(find_label(lv_scr_act(), "Open") == nullptr);
  lv_obj_t *korean_button = lv_obj_get_parent(find_label(lv_scr_act(), "Korean"));
  lv_obj_t *english_button = lv_obj_get_parent(find_label(lv_scr_act(), "English"));
  lv_obj_t *dark_button = lv_obj_get_parent(find_label(lv_scr_act(), "Dark"));
  assert(korean_button != nullptr);
  assert(english_button != nullptr);
  assert(dark_button != nullptr);
  assert_header_button_style(korean_button);
  assert_header_button_style(english_button);
  assert_header_button_style(dark_button);
  assert(lv_obj_get_y(korean_button) == lv_obj_get_y(english_button));
  assert(lv_obj_get_y(english_button) == lv_obj_get_y(dark_button));
  assert(lv_obj_get_x(korean_button) < lv_obj_get_x(english_button));
  assert(lv_obj_get_x(english_button) < lv_obj_get_x(dark_button));
  lv_obj_t *dashboard_brand = lv_obj_get_parent(
      find_label(lv_scr_act(), "Sample Suite"));
  assert(dashboard_brand != nullptr);
  lv_obj_t *dashboard_header = lv_obj_get_parent(dashboard_brand);
  assert(dashboard_header != nullptr);
  lv_obj_t *dashboard_eyebrow =
      find_label(lv_scr_act(), "HMI LAB / COMPONENT PLAYGROUND");
  lv_obj_t *dashboard_description = find_label(
      lv_scr_act(), "A single app for testing reusable interface capabilities.");
  assert(dashboard_eyebrow != nullptr);
  assert(dashboard_description != nullptr);
  lv_area_t brand_area{};
  lv_area_t eyebrow_area{};
  lv_area_t description_area{};
  lv_obj_get_coords(dashboard_brand, &brand_area);
  lv_obj_get_coords(dashboard_eyebrow, &eyebrow_area);
  lv_obj_get_coords(dashboard_description, &description_area);
  const int top_gap = eyebrow_area.y1 - brand_area.y1;
  const int bottom_gap = brand_area.y2 - description_area.y2;
  assert(std::abs(top_gap - bottom_gap) <= 2);
  assert(lv_obj_get_style_bg_grad_dir(dashboard_header, LV_PART_MAIN) ==
         LV_GRAD_DIR_NONE);
  assert(find_label(lv_scr_act(), "Screen language") == nullptr);
  lv_obj_t *ui_basics_card = lv_obj_get_parent(ui_basics_title);
  lv_obj_t *text_input_card = lv_obj_get_parent(text_input_title);
  lv_obj_t *ui_basics_description = find_label(
      lv_scr_act(), "Buttons, overlays, and a scrollable list.");
  assert(ui_basics_card != nullptr);
  assert(text_input_card != nullptr);
  assert(ui_basics_description != nullptr);
  lv_obj_update_layout(ui_basics_card);
  assert(lv_obj_get_y(ui_basics_card) == lv_obj_get_y(text_input_card));
  assert(lv_obj_get_x(ui_basics_card) < lv_obj_get_x(text_input_card));
  lv_area_t dashboard_card_area{};
  lv_area_t dashboard_description_area{};
  lv_obj_get_coords(ui_basics_card, &dashboard_card_area);
  lv_obj_get_coords(ui_basics_description, &dashboard_description_area);
  const lv_coord_t card_bottom_gap =
      dashboard_card_area.y2 - dashboard_description_area.y2;
  assert(card_bottom_gap >= 16);
  assert(card_bottom_gap <= 22);
  assert(find_label(lv_scr_act(), "1 PAGE") == nullptr);
  assert(find_label(lv_scr_act(), "SHARED UI") == nullptr);
  assert(find_label(lv_scr_act(), "WINDOWS + ESP32-P4") == nullptr);
  assert(find_label(lv_scr_act(), "IN-MEMORY THEME") == nullptr);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    const std::string dashboard_path =
        std::string(argv[2]) + ".light-dashboard.bmp";
    assert(port->save_screenshot(dashboard_path.c_str()));
  }
  lv_area_t ui_basics_area{};
  lv_obj_get_coords(ui_basics_card, &ui_basics_area);
  pointer::click_pointer(*port,
                         (ui_basics_area.x1 + ui_basics_area.x2) / 2,
                         (ui_basics_area.y1 + ui_basics_area.y2) / 2);
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    for (int frame = 0; frame < 10; ++frame) {
      port->tick();
    }
    assert(port->save_screenshot(argv[2]));
  }

  pointer::click_pointer(*port, 130, 255);
  assert(app->snapshot().page.last_action == PageAction::kPress);
  pointer::hold_pointer(*port, 355, 255);
  assert(app->snapshot().page.last_action == PageAction::kLongPress);
  lv_obj_t *list_title = find_label(lv_scr_act(), "Scrollable List");
  assert(list_title != nullptr);
  lv_obj_t *list_header = lv_obj_get_parent(list_title);
  assert(list_header != nullptr);
  lv_obj_t *list_panel = lv_obj_get_parent(list_header);
  assert(list_panel != nullptr);
  const lv_coord_t scroll_before = lv_obj_get_scroll_y(list_panel);
  pointer::drag_pointer(*port, 800, 500, 270);
  assert(lv_obj_get_scroll_y(list_panel) > scroll_before);
  send_to_labeled_button(lv_scr_act(), "Show Toast", LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.overlay == OverlayState::kToast);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    for (int frame = 0; frame < 10; ++frame) {
      port->tick();
    }
    const std::string toast_path = std::string(argv[2]) + ".toast.bmp";
    assert(port->save_screenshot(toast_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "Open Dialog", LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.overlay == OverlayState::kConfirm);
  assert(find_label(lv_scr_act(), "CONFIRMATION") == nullptr);
  assert_dialog_uses_full_screen_geometry("Confirm Action");
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    for (int frame = 0; frame < 10; ++frame) {
      port->tick();
    }
    const std::string dialog_path = std::string(argv[2]) + ".dialog.bmp";
    assert(port->save_screenshot(dialog_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "Cancel", LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.last_action == PageAction::kDialogCancelled);
  const auto press_count_before_theme = app->snapshot().page.press_count;
  const auto long_press_count_before_theme =
      app->snapshot().page.long_press_count;

  send_to_labeled_button(lv_scr_act(), "Korean", LV_EVENT_SHORT_CLICKED);
  assert(app->flush_pending_transition());
  assert(app->snapshot().locale == Locale::ko_kr);
  assert(app->snapshot().page.press_count == press_count_before_theme);
  assert(app->snapshot().page.long_press_count ==
         long_press_count_before_theme);
  assert(find_label(lv_scr_act(), "상호작용 제어") != nullptr);
  assert(find_label(lv_scr_act(), "한국어") != nullptr);
  assert(find_label(lv_scr_act(), "영어") != nullptr);
  assert(find_label(lv_scr_act(), "어둡게") != nullptr);
  send_to_labeled_button(lv_scr_act(), "영어", LV_EVENT_SHORT_CLICKED);
  assert(app->flush_pending_transition());
  assert(app->snapshot().locale == Locale::en_us);
  assert(app->snapshot().page.press_count == press_count_before_theme);
  assert(app->snapshot().page.long_press_count ==
         long_press_count_before_theme);
  assert(find_label(lv_scr_act(), "Interaction controls") != nullptr);

  for (int frame = 0; frame < 10; ++frame) {
    port->tick();
  }
  send_to_labeled_button(lv_scr_act(), "Dark",
                         LV_EVENT_SHORT_CLICKED);
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kDark);
  assert(app->snapshot().page.press_count == press_count_before_theme);
  assert(app->snapshot().page.long_press_count ==
         long_press_count_before_theme);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    for (int frame = 0; frame < 10; ++frame) {
      port->tick();
    }
    const std::string dark_path = std::string(argv[2]) + ".dark.bmp";
    assert(port->save_screenshot(dark_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "Show Toast", LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.overlay == OverlayState::kToast);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    for (int frame = 0; frame < 10; ++frame) {
      port->tick();
    }
    const std::string dark_toast_path =
        std::string(argv[2]) + ".dark.toast.bmp";
    assert(port->save_screenshot(dark_toast_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "Open Dialog", LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.overlay == OverlayState::kConfirm);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    for (int frame = 0; frame < 10; ++frame) {
      port->tick();
    }
    const std::string dark_dialog_path =
        std::string(argv[2]) + ".dark.dialog.bmp";
    assert(port->save_screenshot(dark_dialog_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "Cancel", LV_EVENT_SHORT_CLICKED);

  send_to_labeled_button(lv_scr_act(), "Back", LV_EVENT_SHORT_CLICKED);
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kDashboard);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kDark);
  assert(app->snapshot().locale == Locale::en_us);
  assert(find_label(lv_scr_act(), "UI Basics") != nullptr);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    for (int frame = 0; frame < 10; ++frame) {
      port->tick();
    }
    const std::string dark_dashboard_path =
        std::string(argv[2]) + ".dark-dashboard.bmp";
    assert(port->save_screenshot(dark_dashboard_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "Korean", LV_EVENT_SHORT_CLICKED);
  assert(app->flush_pending_transition());
  assert(app->snapshot().locale == Locale::ko_kr);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kDark);
  assert(find_label(lv_scr_act(), "UI 기본") != nullptr);
  assert(find_label(lv_scr_act(), "텍스트 입력") != nullptr);
  assert(app->request_page("ui-basics"));
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(find_label(lv_scr_act(), "상호작용 제어") != nullptr);
  assert(find_label(lv_scr_act(), "스크롤 목록") != nullptr);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    const std::string ko_ui_basics_path =
        std::string(argv[2]) + ".ko-ui-basics.bmp";
    assert(port->save_screenshot(ko_ui_basics_path.c_str()));
  }
  send_to_labeled_button(lv_scr_act(), "토스트 표시",
                         LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.overlay == OverlayState::kToast);
  send_to_labeled_button(lv_scr_act(), "다이얼로그 열기",
                         LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.overlay == OverlayState::kConfirm);
  send_to_labeled_button(lv_scr_act(), "취소", LV_EVENT_SHORT_CLICKED);
  assert(app->snapshot().page.last_action == PageAction::kDialogCancelled);
  app->destroy();
  return 0;
}
