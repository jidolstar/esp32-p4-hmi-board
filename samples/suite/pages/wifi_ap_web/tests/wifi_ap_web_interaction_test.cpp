#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>

#include <lvgl.h>
#include <src/extra/libs/qrcode/lv_qrcode.h>

#include "app.hpp"
#include "connectivity/virtual_wifi_ap.hpp"
#include "connectivity/virtual_wifi_sta.hpp"
#include "hmi/connectivity/wifi_ap_runtime.hpp"
#include "hmi/connectivity/wifi_ap_web_controller.hpp"
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
  const std::uint32_t count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0U; index < count; ++index) {
    if (lv_obj_t* found = find_label(lv_obj_get_child(parent, index), text)) {
      return found;
    }
  }
  return nullptr;
}

lv_obj_t* find_label_contains(lv_obj_t* parent, const char* text) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(parent, &lv_label_class) &&
      std::strstr(lv_label_get_text(parent), text) != nullptr) {
    return parent;
  }
  const std::uint32_t count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0U; index < count; ++index) {
    if (lv_obj_t* found =
            find_label_contains(lv_obj_get_child(parent, index), text)) {
      return found;
    }
  }
  return nullptr;
}

std::size_t count_type(lv_obj_t* parent, const lv_obj_class_t* type) {
  if (parent == nullptr) {
    return 0U;
  }
  std::size_t count = lv_obj_check_type(parent, type) ? 1U : 0U;
  const std::uint32_t children = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0U; index < children; ++index) {
    count += count_type(lv_obj_get_child(parent, index), type);
  }
  return count;
}

void click_label(lv_obj_t* screen, const char* text) {
  lv_obj_t* label = find_label(screen, text);
  assert(label != nullptr);
  lv_obj_t* button = lv_obj_get_parent(label);
  assert(button != nullptr);
  const lv_res_t result = lv_event_send(button, LV_EVENT_SHORT_CLICKED, nullptr);
  assert(result == LV_RES_OK || result == LV_RES_INV);
}

void advance(hmi::connectivity::WifiStaRegistration& station,
             hmi::connectivity::WifiApWebController& access_point,
             std::uint32_t& now_ms, const unsigned count) {
  for (unsigned index = 0U; index < count; ++index) {
    now_ms += 100U;
    lv_tick_inc(100U);
    station.tick(now_ms);
    access_point.tick(now_ms);
    lv_timer_handler();
  }
}

bool fill_random(void* context, std::uint8_t* output,
                 const std::size_t size) noexcept {
  auto& value = *static_cast<std::uint8_t*>(context);
  for (std::size_t index = 0U; index < size; ++index) {
    output[index] = value++;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  using hmi::connectivity::WifiApRuntimeState;
  using hmi::samples::suite::SuiteView;

  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("wifi-ap-web interaction", false));
  hmi::platform::connectivity::VirtualWifiSta station_port;
  hmi::platform::connectivity::MemoryWifiStationSettingsStore store;
  hmi::connectivity::WifiStaRegistration station(station_port, store);
  assert(station.begin(0U, true));
  constexpr char station_ssid[] = "HMI-LAB";
  constexpr char station_password[] = "starter123";
  assert(station.connect_service(
      reinterpret_cast<const std::uint8_t*>(station_ssid),
      sizeof(station_ssid) - 1U,
      reinterpret_cast<const std::uint8_t*>(station_password),
      sizeof(station_password) - 1U,
      hmi::connectivity::WifiSecurity::personal));
  std::uint32_t now_ms = 0U;
  for (unsigned index = 0U; index < 6U; ++index) {
    now_ms += 100U;
    station.tick(now_ms);
  }
  assert(station.snapshot().has_ip);
  hmi::platform::connectivity::VirtualWifiAp ap_port;
  hmi::connectivity::WifiApRuntime access_point(
      station, ap_port,
      hmi::connectivity::WifiApStationPolicy::keep_connected);
  std::uint8_t random_seed = 0U;
  hmi::connectivity::WifiApWebSessionSource source{};
  source.random = {&random_seed, &fill_random};
  std::memcpy(source.device_suffix, "BEEF", 5U);
  hmi::connectivity::WifiApWebController web_controller(access_point, source);

  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  hmi::profiles::AppContext context;
  context.host_name = "windows";
  context.wifi_registration = &station;
  context.ap_control = &web_controller;
  assert(app->create(context));
  assert(app->request_page("wifi-ap-web"));
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(find_label(lv_scr_act(), "Wi-Fi AP 웹 제어") != nullptr);
  assert(find_label(lv_scr_act(), "AP 꺼짐") != nullptr);
  assert(find_label(lv_scr_act(), "AP 시작") != nullptr);
  const char* capture_state = nullptr;
  const char* capture_path = nullptr;
  if (argc == 4 && std::strcmp(argv[1], "--capture") == 0) {
    capture_state = argv[2];
    capture_path = argv[3];
  } else if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    capture_state = "control-ko-light";
    capture_path = argv[2];
  }
  const auto capture = [&](const char* state) {
    if (capture_state != nullptr && std::strcmp(capture_state, state) == 0) {
      lv_obj_update_layout(lv_scr_act());
      lv_obj_invalidate(lv_scr_act());
      lv_refr_now(nullptr);
      assert(lvgl->save_screenshot(capture_path));
    }
  };
  capture("off-ko-light");
  if (capture_state != nullptr &&
      std::strcmp(capture_state, "error-ko-light") == 0) {
    ap_port.set_scenario(
        hmi::platform::connectivity::VirtualWifiApScenario::start_failure);
    click_label(lv_scr_act(), "AP 시작");
    advance(station, web_controller, now_ms, 2U);
    assert(find_label(lv_scr_act(), "AP를 시작할 수 없음") != nullptr);
    capture("error-ko-light");
    app->destroy();
    return 0;
  }

  click_label(lv_scr_act(), "AP 시작");
  advance(station, web_controller, now_ms, 4U);
  assert(access_point.snapshot().state == WifiApRuntimeState::ready);
  assert(access_point.snapshot().sta_has_ip);
  assert(station.snapshot().has_ip);
  assert(find_label(lv_scr_act(), "1단계 · Wi-Fi 연결") != nullptr);
  assert(find_label_contains(lv_scr_act(), "HMI-Control-BEEF") != nullptr);
  assert(count_type(lv_scr_act(), &lv_qrcode_class) >= 1U);
  capture("wait-ko-light");

  ap_port.set_client_count(1U);
  advance(station, web_controller, now_ms, 2U);
  assert(find_label(lv_scr_act(), "2단계 · 웹 열기") != nullptr);
  assert(find_label(lv_scr_act(), "Wi-Fi QR 다시 보기") != nullptr);
  char token[40]{};
  web_controller.copy_session_token(token, sizeof(token));
  char auth[64]{};
  std::snprintf(auth, sizeof(auth), "{\"token\":\"%s\"}", token);
  assert(web_controller.open_web_client(7U));
  assert(web_controller.receive_web_frame(7U, auth, std::strlen(auth)) ==
         hmi::connectivity::WifiApWebFrameStatus::authenticated);
  advance(station, web_controller, now_ms, 2U);
  assert(find_label(lv_scr_act(), "3단계 · 웹 연결됨") != nullptr);

  click_label(lv_scr_act(), "스위치 1");
  assert(web_controller.snapshot().revision == 1U);
  assert(web_controller.snapshot().switch_values[0]);
  assert(find_label(lv_scr_act(), "켜짐") != nullptr);
  click_label(lv_scr_act(), "Wi-Fi QR 다시 보기");
  assert(find_label(lv_scr_act(), "웹 QR 보기") != nullptr);
  click_label(lv_scr_act(), "웹 QR 보기");

  web_controller.close_web_client(7U);
  advance(station, web_controller, now_ms, 2U);
  assert(find_label(lv_scr_act(), "웹 재연결 대기") != nullptr);
  assert(web_controller.open_web_client(8U));
  assert(web_controller.receive_web_frame(8U, auth, std::strlen(auth)) ==
         hmi::connectivity::WifiApWebFrameStatus::authenticated);
  advance(station, web_controller, now_ms, 2U);
  assert(find_label(lv_scr_act(), "3단계 · 웹 연결됨") != nullptr);
  assert(web_controller.snapshot().revision == 1U);
  assert(web_controller.snapshot().switch_values[0]);
  capture("control-ko-light");

  click_label(lv_scr_act(), "영어");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Wi-Fi AP Web Control") != nullptr);
  assert(find_label(lv_scr_act(), "Step 3 · Web Connected") != nullptr);
  assert(web_controller.snapshot().revision == 1U);
  click_label(lv_scr_act(), "Dark");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Light") != nullptr);
  assert(find_label(lv_scr_act(), "Step 3 · Web Connected") != nullptr);
  assert(web_controller.snapshot().revision == 1U);
  capture("control-en-dark");

  web_controller.close_web_client(8U);
  advance(station, web_controller, now_ms, 2U);
  assert(find_label(lv_scr_act(), "Waiting for Web Reconnect") != nullptr);
  capture("reconnect-en-dark");
  assert(web_controller.open_web_client(9U));
  assert(web_controller.receive_web_frame(9U, auth, std::strlen(auth)) ==
         hmi::connectivity::WifiApWebFrameStatus::authenticated);
  advance(station, web_controller, now_ms, 2U);

  click_label(lv_scr_act(), "Back");
  lv_obj_t* confirm_title = find_label(lv_scr_act(), "Stop the AP?");
  assert(confirm_title != nullptr);
  lv_obj_t* dialog = lv_obj_get_parent(confirm_title);
  lv_obj_t* scrim = lv_obj_get_parent(dialog);
  assert(lv_obj_get_parent(scrim) == lv_scr_act());
  lv_obj_update_layout(lv_scr_act());
  assert(lv_obj_get_x(scrim) == 0);
  assert(lv_obj_get_y(scrim) == 0);
  assert(lv_obj_get_width(scrim) == lv_disp_get_hor_res(nullptr));
  assert(lv_obj_get_height(scrim) == lv_disp_get_ver_res(nullptr));
  capture("confirm-en-dark");
  click_label(lv_scr_act(), "Cancel");
  assert(find_label(lv_scr_act(), "Stop the AP?") == nullptr);
  assert(access_point.snapshot().state == WifiApRuntimeState::ready);
  click_label(lv_scr_act(), "Back");
  click_label(lv_scr_act(), "Stop");
  advance(station, web_controller, now_ms, 5U);
  assert(access_point.snapshot().state == WifiApRuntimeState::off);
  assert(station.snapshot().has_ip);
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kDashboard);

  // 최종 app destroy는 page가 active인 경우에도 AP/session을 즉시 정리한다.
  assert(app->request_page("wifi-ap-web"));
  assert(app->flush_pending_transition());
  click_label(lv_scr_act(), "Start AP");
  advance(station, web_controller, now_ms, 4U);
  assert(access_point.snapshot().state == WifiApRuntimeState::ready);
  app->destroy();
  assert(access_point.snapshot().state == WifiApRuntimeState::off);
  return 0;
}
