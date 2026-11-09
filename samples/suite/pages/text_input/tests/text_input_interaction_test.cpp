#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include <lvgl.h>
#include <src/extra/libs/fsdrv/lv_fsdrv.h>

#include "app.hpp"
#include "hmi/ui/font_repository.hpp"
#include "pointer_test_driver.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

void* load_font(void*, const char* relative_path, std::uint32_t size_bytes,
                std::uint32_t crc32) noexcept {
  std::array<char, 320U> path{};
  const int written = std::snprintf(path.data(), path.size(),
                                    "A:samples/suite/%s", relative_path);
  if (written <= 0 || static_cast<std::size_t>(written) >= path.size()) {
    return nullptr;
  }
  return hmi::ui::load_binary_font_file(path.data(), size_bytes, crc32);
}

void free_font(void*, void* font) noexcept {
  lv_font_free(static_cast<lv_font_t*>(font));
}

void* fail_font_load(void*, const char*, std::uint32_t,
                     std::uint32_t) noexcept {
  return nullptr;
}

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
    if (lv_obj_t* found = find_label(lv_obj_get_child(parent, index), text)) {
      return found;
    }
  }
  return nullptr;
}

void click_label(hmi::platform::windows::SdlLvglPort& port, const char* text) {
  lv_obj_t* label = find_label(lv_scr_act(), text);
  assert(label != nullptr);
  lv_obj_t* target = lv_obj_get_parent(label);
  assert(target != nullptr);
  lv_area_t area{};
  lv_obj_get_coords(target, &area);
  hmi::platform::windows::testing::click_pointer(
      port, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2);
}

void settle(hmi::platform::windows::SdlLvglPort& port) {
  for (int frame = 0; frame < 8; ++frame) {
    port.tick();
  }
}

void count_deleted_key(lv_event_t* event) {
  auto* count = static_cast<unsigned*>(lv_event_get_user_data(event));
  if (count != nullptr && lv_event_get_code(event) == LV_EVENT_DELETE) {
    ++(*count);
  }
}

void assert_review_dialog_uses_full_screen_geometry() {
  lv_obj_t* title = find_label(lv_scr_act(), "입력 확인");
  assert(title != nullptr);
  lv_obj_t* dialog = lv_obj_get_parent(title);
  lv_obj_t* scrim = lv_obj_get_parent(dialog);
  lv_obj_t* content = lv_obj_get_parent(scrim);
  assert(dialog != nullptr && scrim != nullptr && content != nullptr);

  lv_area_t content_area{};
  lv_area_t scrim_area{};
  lv_area_t dialog_area{};
  lv_obj_get_coords(content, &content_area);
  lv_obj_get_coords(scrim, &scrim_area);
  lv_obj_get_coords(dialog, &dialog_area);

  // dim layer가 header를 포함한 화면 전체를 덮고, dialog가 그 중앙에 있어야 한다.
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

int main(int argc, char** argv) {
  using hmi::i18n::Locale;
  using hmi::samples::suite::SampleSuiteApp;
  using hmi::samples::suite::SuiteView;

  lv_init();
  lv_fs_stdio_init();
  hmi::platform::windows::SdlLvglPort port;
  assert(port.begin("text input interaction", argc > 1));
  SampleSuiteApp app;
  hmi::profiles::AppContext context;
  context.host_name = "windows";
  context.fonts = {nullptr, &load_font, &free_font};
  assert(app.create(context));
  assert(app.request_page("text-input"));
  assert(app.flush_pending_transition());
  assert(app.snapshot().view == SuiteView::kPage);
  assert(find_label(lv_scr_act(), "화면 언어") == nullptr);
  settle(port);
  assert(find_label(lv_scr_act(), "텍스트 입력") != nullptr);

  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    const std::string path = std::string(argv[2]) + ".ko.bmp";
    assert(port.save_screenshot(path.c_str()));
  }

  // 일반 문자 입력은 기존 keyboard tree를 유지해야 한다.
  unsigned deleted_keys = 0U;
  lv_obj_t* watched_key_label = find_label(lv_scr_act(), "ㅏ");
  assert(watched_key_label != nullptr);
  lv_obj_t* watched_key = lv_obj_get_parent(watched_key_label);
  assert(watched_key != nullptr);
  lv_obj_add_event_cb(watched_key, &count_deleted_key, LV_EVENT_DELETE,
                      &deleted_keys);
  click_label(port, "ㅎ");
  assert(deleted_keys == 0U);
  click_label(port, "ㅏ");
  assert(deleted_keys == 0U);
  click_label(port, "ㄴ");
  click_label(port, "다음");
  click_label(port, "한/영");
  click_label(port, "m");
  click_label(port, "e");
  click_label(port, "다음");
  click_label(port, "1");
  click_label(port, "공백");
  click_label(port, "2");
  click_label(port, "입력완료");
  assert(find_label(lv_scr_act(), "입력 확인") != nullptr);
  assert(find_label(lv_scr_act(), "한") != nullptr);
  assert_review_dialog_uses_full_screen_geometry();
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    settle(port);
    const std::string path = std::string(argv[2]) + ".review.bmp";
    assert(port.save_screenshot(path.c_str()));
  }
  click_label(port, "수정");
  click_label(port, "3");
  click_label(port, "입력완료");
  click_label(port, "확인");
  assert(find_label(lv_scr_act(), "입력 내용을 확인했습니다") != nullptr);

  // theme widget 재생성 뒤에도 입력값과 확인 상태가 유지된다.
  assert(app.request_theme_toggle());
  assert(app.flush_pending_transition());
  assert(find_label(lv_scr_act(), "한") != nullptr);
  assert(find_label(lv_scr_act(), "입력 내용을 확인했습니다") != nullptr);
  click_label(port, "영어");
  assert(app.snapshot().locale == Locale::en_us);
  assert(find_label(lv_scr_act(), "Text Input") != nullptr);
  assert(find_label(lv_scr_act(), "Input reviewed") != nullptr);
  settle(port);

  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    const std::string path = std::string(argv[2]) + ".en.bmp";
    assert(port.save_screenshot(path.c_str()));
  }

  assert(app.request_dashboard());
  assert(app.flush_pending_transition());
  assert(app.request_page("text-input"));
  assert(app.flush_pending_transition());
  assert(app.snapshot().locale == Locale::en_us);
  assert(find_label(lv_scr_act(), "Text Input") != nullptr);
  assert(find_label(lv_scr_act(), "|") == nullptr);
  assert(find_label(lv_scr_act(), "Enter a memo") != nullptr);
  app.destroy();

  // SD font asset이 없어도 page 진입과 Back 복구 동선은 유지돼야 한다.
  SampleSuiteApp recovery_app;
  context.fonts = {nullptr, &fail_font_load, &free_font};
  assert(recovery_app.create(context));
  assert(recovery_app.request_page("text-input"));
  assert(recovery_app.flush_pending_transition());
  assert(recovery_app.snapshot().view == SuiteView::kPage);
  assert(find_label(lv_scr_act(), "SD font assets unavailable") != nullptr);
  assert(recovery_app.request_dashboard());
  assert(recovery_app.flush_pending_transition());
  assert(recovery_app.snapshot().view == SuiteView::kDashboard);
  recovery_app.destroy();
  return 0;
}

