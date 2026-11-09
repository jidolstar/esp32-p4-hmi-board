#include <cassert>
#include <memory>

#include <lvgl.h>

#include "hmi/storage/storage_access_controller.hpp"
#include "pages/sd_storage/sd_storage_page.hpp"
#include "sdl_lvgl_port.hpp"
#include "storage/virtual_storage_access.hpp"

namespace {

void ignore(void*) noexcept {}
bool accept_locale(void*, hmi::i18n::Locale) noexcept { return true; }

struct LockRefreshProbe {
  unsigned count = 0U;
};

void refresh_lock(void* context) noexcept {
  ++static_cast<LockRefreshProbe*>(context)->count;
}

}  // namespace

int main() {
  using hmi::samples::suite::SdStoragePage;
  using hmi::storage::StorageAccessState;

  lv_init();
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(port->begin("sd storage state", false));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  hmi::platform::windows::storage::VirtualStorageAccess storage_port;
  hmi::storage::StorageAccessController controller(storage_port);
  controller.tick(0U);
  hmi::ui::UiTheme theme;
  assert(theme.initialize());
  LockRefreshProbe probe;
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &ignore;
  context.request_theme_toggle = &ignore;
  context.refresh_storage_lock = &refresh_lock;
  context.set_locale = &accept_locale;
  context.storage_access = &controller;
  context.theme = &theme;
  context.user_data = &probe;

  SdStoragePage page;
  assert(page.create(screen, context));
  auto value = page.storage_snapshot();
  assert(value.state == StorageAccessState::hmi_ready);
  assert(value.media_present);
  assert(value.connect_enabled);
  assert(value.used_percent == 25U);
  assert(page.request_pc_access());
  assert(probe.count == 1U);
  assert(page.storage_snapshot().state == StorageAccessState::quiescing);
  assert(!page.storage_snapshot().connect_enabled);
  controller.tick(10U);
  assert(storage_port.complete_pending());
  controller.tick(20U);
  page.refresh();
  assert(page.storage_snapshot().state == StorageAccessState::waiting_for_host);
  assert(page.storage_snapshot().owner == hmi::storage::StorageOwner::usb_msc);
  assert(!page.request_pc_access());

  page.destroy();

  hmi::platform::windows::storage::VirtualStorageAccess empty_port;
  empty_port.set_media_present(false);
  hmi::storage::StorageAccessController empty_controller(empty_port);
  empty_controller.tick(0U);
  context.storage_access = &empty_controller;
  assert(page.create(screen, context));
  assert(page.storage_snapshot().state == StorageAccessState::unavailable);
  assert(!page.storage_snapshot().connect_enabled);
  assert(page.retry_media());
  assert(!empty_controller.snapshot().lock_required);
  empty_controller.tick(1U);
  assert(empty_port.complete_pending());
  empty_controller.tick(2U);
  page.refresh();
  assert(page.storage_snapshot().state == StorageAccessState::hmi_ready);
  assert(page.storage_snapshot().connect_enabled);
  page.destroy();

  lv_obj_del(screen);
  theme.reset();
  return 0;
}
