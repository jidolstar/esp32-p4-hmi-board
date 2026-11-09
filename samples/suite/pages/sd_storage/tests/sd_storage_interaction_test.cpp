#include <cassert>
#include <cstring>
#include <memory>
#include <string>

#include <lvgl.h>

#include "app.hpp"
#include "hmi/storage/storage_access_controller.hpp"
#include "profiles/runtime/app_contract.hpp"
#include "sdl_lvgl_port.hpp"
#include "storage/virtual_storage_access.hpp"

namespace {

lv_obj_t* find_label(lv_obj_t* root, const char* text) {
  if (root == nullptr || text == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(root, &lv_label_class) &&
      std::strcmp(lv_label_get_text(root), text) == 0) {
    return root;
  }
  const std::uint32_t count = lv_obj_get_child_cnt(root);
  for (std::uint32_t index = 0; index < count; ++index) {
    if (lv_obj_t* found = find_label(lv_obj_get_child(root, index), text)) {
      return found;
    }
  }
  return nullptr;
}

void press_button(const char* text) {
  lv_obj_t* label = find_label(lv_scr_act(), text);
  assert(label != nullptr);
  lv_obj_t* button = lv_obj_get_parent(label);
  assert(button != nullptr);
  lv_event_send(button, LV_EVENT_SHORT_CLICKED, nullptr);
}

void refresh_screen() {
  lv_obj_update_layout(lv_scr_act());
  for (unsigned pass = 0U; pass < 2U; ++pass) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
  }
}

void finish_expose(
    hmi::storage::StorageAccessController& controller,
    hmi::platform::windows::storage::VirtualStorageAccess& port,
    std::uint32_t& now_ms) {
  controller.tick(++now_ms);
  assert(port.complete_pending());
  controller.tick(++now_ms);
}

void finish_reclaim(
    hmi::storage::StorageAccessController& controller,
    hmi::platform::windows::storage::VirtualStorageAccess& port,
    std::uint32_t& now_ms) {
  controller.tick(++now_ms);
  assert(port.complete_pending());
  controller.tick(++now_ms);
}

}  // namespace

int main(int argc, char** argv) {
  using hmi::samples::suite::SampleSuiteApp;
  using hmi::samples::suite::SuiteView;
  using hmi::storage::StorageAccessState;

  lv_init();
  auto lvgl_port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl_port->begin("sd storage interaction", false));
  hmi::platform::windows::storage::VirtualStorageAccess storage_port;
  hmi::storage::StorageAccessController controller(storage_port);
  std::uint32_t now_ms = 0U;
  controller.tick(now_ms);

  hmi::profiles::AppContext context;
  context.host_name = "windows-test";
  context.storage_access = &controller;
  auto app = std::make_unique<SampleSuiteApp>();
  assert(app->create(context));
  assert(app->request_page("sd-storage"));
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(std::strcmp(app->snapshot().active_page_id, "sd-storage") == 0);
  const char* screenshot_base =
      argc == 3 && std::strcmp(argv[1], "--screenshot") == 0 ? argv[2]
                                                               : nullptr;
  if (screenshot_base != nullptr) {
    refresh_screen();
    assert(lvgl_port->save_screenshot(
        (std::string(screenshot_base) + ".ko-page.bmp").c_str()));
  }

  // 잠금이 시작되면 이미 예약된 navigation을 폐기하고 모든 shell intent를 거부한다.
  assert(app->request_page("wifi"));
  assert(controller.request_pc_access(now_ms));
  app->refresh_storage_access_lock();
  assert(app->snapshot().storage_lock.active);
  assert(!app->flush_pending_transition());
  assert(std::strcmp(app->snapshot().active_page_id, "sd-storage") == 0);
  assert(!app->request_dashboard());
  assert(!app->request_page("ui-basics"));
  assert(!app->request_theme_toggle());
  assert(!app->handle_utf8_input("blocked"));
  assert(!app->handle_key(hmi::profiles::AppKey::enter));
  press_button("영어");
  assert(app->snapshot().locale == hmi::i18n::Locale::ko_kr);

  finish_expose(controller, storage_port, now_ms);
  app->refresh_storage_access_lock();
  assert(controller.snapshot().state == StorageAccessState::waiting_for_host);
  assert(find_label(lv_scr_act(), "HMI로 돌아가기") != nullptr);
  if (screenshot_base != nullptr) {
    refresh_screen();
    assert(lvgl_port->save_screenshot(
        (std::string(screenshot_base) + ".ko-lock.bmp").c_str()));
  }
  press_button("HMI로 돌아가기");
  assert(app->snapshot().storage_lock.return_confirmation);
  press_button("연결 끊고 돌아가기");
  assert(controller.snapshot().state == StorageAccessState::reclaiming);
  finish_reclaim(controller, storage_port, now_ms);
  app->refresh_storage_access_lock();
  assert(!app->snapshot().storage_lock.active);

  // 실제 host 접근 뒤 cable unplug가 grace를 지나면 자동 복귀한다.
  assert(controller.request_pc_access(now_ms));
  app->refresh_storage_access_lock();
  finish_expose(controller, storage_port, now_ms);
  storage_port.host_read();
  controller.tick(++now_ms);
  storage_port.usb_unplug();
  controller.tick(++now_ms);
  assert(controller.snapshot().state == StorageAccessState::pc_active);
  now_ms += 200U;
  controller.tick(now_ms);
  assert(controller.snapshot().state == StorageAccessState::reclaiming);
  finish_reclaim(controller, storage_port, now_ms);
  app->refresh_storage_access_lock();
  assert(!app->snapshot().storage_lock.active);
  assert(std::strcmp(app->snapshot().active_page_id, "sd-storage") == 0);

  // PC eject는 사용자 입력 없이 동일 page로 자동 복귀한다.
  assert(controller.request_pc_access(now_ms));
  app->refresh_storage_access_lock();
  finish_expose(controller, storage_port, now_ms);
  storage_port.host_read();
  controller.tick(++now_ms);
  assert(controller.snapshot().state == StorageAccessState::pc_active);
  storage_port.host_eject();
  controller.tick(++now_ms);
  assert(controller.snapshot().state == StorageAccessState::reclaiming);
  finish_reclaim(controller, storage_port, now_ms);
  app->refresh_storage_access_lock();
  assert(!app->snapshot().storage_lock.active);

  // remount 실패는 잠금을 유지하고 retry 성공에서만 앱을 다시 연다.
  assert(controller.request_pc_access(now_ms));
  app->refresh_storage_access_lock();
  finish_expose(controller, storage_port, now_ms);
  assert(controller.request_hmi_access(
      now_ms, hmi::storage::StorageReclaimReason::explicit_request));
  controller.tick(++now_ms);
  storage_port.set_next_result(hmi::storage::StoragePortResult::remount_failed);
  assert(storage_port.complete_pending());
  controller.tick(++now_ms);
  app->refresh_storage_access_lock();
  assert(controller.snapshot().state == StorageAccessState::remount_failed);
  assert(app->snapshot().storage_lock.active);
  assert(!app->request_dashboard());
  press_button("다시 시도");
  controller.tick(++now_ms);
  assert(storage_port.complete_pending());
  controller.tick(++now_ms);
  app->refresh_storage_access_lock();
  assert(controller.snapshot().state == StorageAccessState::hmi_ready);
  assert(!app->snapshot().storage_lock.active);
  assert(controller.snapshot().rescan_revision == 4U);

  if (screenshot_base != nullptr) {
    press_button("영어");
    assert(app->flush_pending_transition());
    refresh_screen();
    assert(lvgl_port->save_screenshot(
        (std::string(screenshot_base) + ".en-page.bmp").c_str()));
    assert(controller.request_pc_access(now_ms));
    app->refresh_storage_access_lock();
    finish_expose(controller, storage_port, now_ms);
    app->refresh_storage_access_lock();
    refresh_screen();
    assert(lvgl_port->save_screenshot(
        (std::string(screenshot_base) + ".en-lock.bmp").c_str()));
    assert(controller.request_hmi_access(
        now_ms, hmi::storage::StorageReclaimReason::explicit_request));
    finish_reclaim(controller, storage_port, now_ms);
    app->refresh_storage_access_lock();
  }

  app->destroy();
  return 0;
}
