#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

#include <lvgl.h>
#include <src/extra/libs/fsdrv/lv_fsdrv.h>

#include "app.hpp"
#include "connectivity/virtual_wifi_sta.hpp"
#include "hmi/connectivity/wifi_sta_registration.hpp"
#include "hmi/ui/font_repository.hpp"
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

lv_obj_t* find_label_prefix(lv_obj_t* parent, const char* prefix) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(parent, &lv_label_class) &&
      std::strncmp(lv_label_get_text(parent), prefix, std::strlen(prefix)) == 0) {
    return parent;
  }
  const std::uint32_t count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0U; index < count; ++index) {
    if (lv_obj_t* found =
            find_label_prefix(lv_obj_get_child(parent, index), prefix)) {
      return found;
    }
  }
  return nullptr;
}

void click_label(lv_obj_t* screen, const char* text) {
  lv_obj_t* label = find_label(screen, text);
  assert(label != nullptr);
  lv_obj_t* target = label;
  while (target != nullptr &&
         !lv_obj_has_flag(target, LV_OBJ_FLAG_CLICKABLE)) {
    target = lv_obj_get_parent(target);
  }
  assert(target != nullptr);
  // 한/영 전환처럼 callback이 event target이 포함된 keyboard를
  // 즉시 재구성하면 정상 처리 후 LV_RES_INV를 반환할 수 있다.
  const lv_res_t result = lv_event_send(target, LV_EVENT_CLICKED, nullptr);
  assert(result == LV_RES_OK || result == LV_RES_INV);
}

void advance(hmi::connectivity::WifiStaRegistration& registration,
             hmi::platform::windows::SdlLvglPort& lvgl,
             std::uint32_t& now_ms, unsigned count) {
  for (unsigned index = 0U; index < count; ++index) {
    now_ms += 200U;
    registration.tick(now_ms);
    lv_tick_inc(200U);
    (void)lv_timer_handler();
    lvgl.tick();
  }
}

void save_requested_screenshot(int argc, char** argv, const char* option,
                               hmi::platform::windows::SdlLvglPort& lvgl) {
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::strcmp(argv[index], option) == 0) {
      lv_obj_invalidate(lv_scr_act());
      lv_refr_now(nullptr);
      assert(lvgl.save_screenshot(argv[index + 1]));
      return;
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  lv_init();
  lv_fs_stdio_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("wifi settings interaction", false));
  hmi::platform::connectivity::VirtualWifiSta wifi;
  hmi::platform::connectivity::MemoryWifiStationSettingsStore store;
  hmi::connectivity::WifiStaRegistration registration(wifi, store);
  std::uint32_t now_ms = 0U;
  assert(registration.begin(now_ms, true));

  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  hmi::profiles::AppContext context;
  context.host_name = "windows";
  context.fonts = {nullptr, &load_font, &free_font};
  context.wifi_registration = &registration;
  assert(app->create(context));
  assert(app->request_page("wifi"));
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Wi-Fi 설정") != nullptr);
  assert(find_label(lv_scr_act(), "주변 네트워크") != nullptr);
  assert(find_label(lv_scr_act(), "네트워크 직접 입력") != nullptr);
  lv_obj_t* korean_button = lv_obj_get_parent(find_label(lv_scr_act(), "한국어"));
  lv_obj_t* english_button = lv_obj_get_parent(find_label(lv_scr_act(), "영어"));
  assert(korean_button != nullptr && english_button != nullptr);
  assert(lv_obj_get_style_bg_opa(korean_button, LV_PART_MAIN) == LV_OPA_COVER);
  assert(lv_obj_get_style_bg_opa(english_button, LV_PART_MAIN) == LV_OPA_TRANSP);

  click_label(lv_scr_act(), "어둡게");
  assert(app->flush_pending_transition());
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kDark);
  assert(find_label(lv_scr_act(), "밝게") != nullptr);

  click_label(lv_scr_act(), "새로고침");
  save_requested_screenshot(argc, argv, "--screenshot-loading", *lvgl);
  advance(registration, *lvgl, now_ms, 6U);
  save_requested_screenshot(argc, argv, "--screenshot-list", *lvgl);
  lv_obj_t* meta = find_label_prefix(lv_scr_act(), "보안");
  assert(meta != nullptr);
  lv_obj_update_layout(lv_scr_act());
  const lv_font_t* meta_font = lv_obj_get_style_text_font(meta, 0);
  assert(meta_font != nullptr);
  assert(lv_obj_get_height(meta) >= meta_font->line_height);
  lv_area_t meta_area{};
  lv_area_t text_group_area{};
  lv_obj_get_coords(meta, &meta_area);
  lv_obj_get_coords(lv_obj_get_parent(meta), &text_group_area);
  assert(meta_area.y1 >= text_group_area.y1);
  assert(meta_area.y2 <= text_group_area.y2);
  lv_obj_t* network = find_label_prefix(lv_scr_act(), "HMI-LAB");
  assert(network != nullptr);
  lv_obj_t* network_heading = find_label(lv_scr_act(), "주변 네트워크");
  assert(network_heading != nullptr);
  assert(lv_color_to32(lv_obj_get_style_text_color(network, 0)) ==
         lv_color_to32(lv_obj_get_style_text_color(network_heading, 0)));
  click_label(lv_scr_act(), "HMI-LAB");
  assert(find_label(lv_scr_act(), "선택한 네트워크") != nullptr);
  assert(find_label(lv_scr_act(), "비밀번호") != nullptr);
  assert(find_label(lv_scr_act(),
                    "연결할 네트워크를 선택하거나 SSID를 입력하세요.") == nullptr);
  click_label(lv_scr_act(), "목록으로");
  assert(find_label(lv_scr_act(), "연결할 네트워크를 선택하세요.") != nullptr);
  network = find_label_prefix(lv_scr_act(), "HMI-LAB");
  assert(network != nullptr);
  click_label(lv_scr_act(), "HMI-LAB");
  assert(find_label(lv_scr_act(), "선택한 네트워크") != nullptr);
  save_requested_screenshot(argc, argv, "--screenshot-editor", *lvgl);
  click_label(lv_scr_act(), "한/영");
  assert(find_label(lv_scr_act(), "ㅂ") != nullptr);
  click_label(lv_scr_act(), "한/영");
  assert(find_label(lv_scr_act(), "q") != nullptr);
  assert(app->handle_utf8_input("wrongpass"));
  click_label(lv_scr_act(), "연결");
  assert(registration.snapshot().state ==
         hmi::connectivity::WifiRegistrationState::connecting);
  assert(find_label(lv_scr_act(), "네트워크에 연결하고 있습니다.") != nullptr);
  assert(find_label(lv_scr_act(), "선택한 네트워크") != nullptr);
  save_requested_screenshot(argc, argv, "--screenshot-connecting", *lvgl);
  advance(registration, *lvgl, now_ms, 6U);
  assert(registration.snapshot().state ==
         hmi::connectivity::WifiRegistrationState::failed);
  assert(find_label(lv_scr_act(), "비밀번호가 올바르지 않습니다.") != nullptr);
  assert(find_label(lv_scr_act(), "선택한 네트워크") != nullptr);
  save_requested_screenshot(argc, argv, "--screenshot-failed", *lvgl);

  assert(app->handle_utf8_input("starter123"));
  click_label(lv_scr_act(), "연결");
  assert(find_label(lv_scr_act(), "네트워크에 연결하고 있습니다.") != nullptr);
  advance(registration, *lvgl, now_ms, 8U);
  assert(registration.snapshot().state ==
         hmi::connectivity::WifiRegistrationState::saved);
  assert(find_label(lv_scr_act(),
                    "네트워크 연결과 설정 저장이 완료되었습니다.") != nullptr);
  assert(find_label(lv_scr_act(), "연결됨") != nullptr);
  save_requested_screenshot(argc, argv, "--screenshot", *lvgl);

  click_label(lv_scr_act(), "영어");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Wi-Fi Settings") != nullptr);
  assert(find_label(lv_scr_act(), "Nearby networks") != nullptr);
  korean_button = lv_obj_get_parent(find_label(lv_scr_act(), "Korean"));
  english_button = lv_obj_get_parent(find_label(lv_scr_act(), "English"));
  assert(korean_button != nullptr && english_button != nullptr);
  assert(lv_obj_get_style_bg_opa(korean_button, LV_PART_MAIN) == LV_OPA_TRANSP);
  assert(lv_obj_get_style_bg_opa(english_button, LV_PART_MAIN) == LV_OPA_COVER);
  click_label(lv_scr_act(), "Back");
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == hmi::samples::suite::SuiteView::kDashboard);
  assert(registration.snapshot().has_ip);
  app->destroy();
  return 0;
}
