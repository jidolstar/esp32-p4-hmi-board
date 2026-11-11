#include <cassert>
#include <memory>

#include <lvgl.h>

#include "pages/date_time/date_time_page.hpp"
#include "sdl_lvgl_port.hpp"
#include "time/virtual_time_sync_ports.hpp"

namespace {

void ignore_back(void*) noexcept {}
void ignore_theme(void*) noexcept {}
bool accept_locale(void*, hmi::i18n::Locale) noexcept { return true; }

}  // namespace

int main() {
  using hmi::samples::suite::DateTimeField;
  using hmi::samples::suite::DateTimePage;
  using hmi::time::ManualApplyResult;

  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("date-time state", false));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  hmi::platform::windows::time::VirtualTimeSyncPorts virtual_time;
  virtual_time.set_rtc({2024U, 2U, 29U, 23U, 58U, 30U});
  hmi::time::DateTimeModel model;
  hmi::time::TimeSyncController controller;
  model.begin(100U);
  auto ports = virtual_time.ports();
  ports.update = hmi::time::date_time_model_sink(model);
  assert(controller.begin(ports, 100U));
  hmi::time::TimeRuntime runtime{&model, &controller};

  hmi::ui::UiTheme theme;
  assert(theme.initialize());
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &ignore_back;
  context.request_theme_toggle = &ignore_theme;
  context.set_locale = &accept_locale;
  context.theme = &theme;
  context.time_runtime = &runtime;

  DateTimePage page;
  assert(page.create(screen, context));
  auto snapshot = page.date_time_snapshot();
  assert(snapshot.has_current);
  assert(snapshot.current.year == 2024U && snapshot.current.month == 2U &&
         snapshot.current.day == 29U);
  assert(snapshot.draft.second == 30U);

  assert(page.set_draft({2024U, 1U, 31U, 12U, 34U, 56U}));
  assert(page.adjust(DateTimeField::month, 1));
  snapshot = page.date_time_snapshot();
  assert(snapshot.draft.month == 2U && snapshot.draft.day == 29U);
  assert(!page.set_draft({2023U, 2U, 29U, 0U, 0U, 0U}));
  assert(page.date_time_snapshot().last_apply ==
         ManualApplyResult::invalid_value);

  assert(page.set_draft({2099U, 12U, 31U, 23U, 59U, 59U}));
  assert(page.adjust(DateTimeField::year, 1));
  assert(page.date_time_snapshot().draft.year == 2099U);
  assert(page.adjust(DateTimeField::second, 1));
  snapshot = page.date_time_snapshot();
  assert(snapshot.draft.year == 2099U && snapshot.draft.month == 12U &&
         snapshot.draft.day == 31U && snapshot.draft.hour == 23U &&
         snapshot.draft.minute == 59U && snapshot.draft.second == 0U);
  assert(page.set_draft({2028U, 2U, 29U, 12U, 0U, 58U}));
  page.advance_draft_seconds(3U);
  snapshot = page.date_time_snapshot();
  assert(snapshot.draft.minute == 1U && snapshot.draft.second == 1U);

  assert(page.set_draft({2025U, 12U, 31U, 9U, 7U, 55U}));
  virtual_time.fail_rtc_write = true;
  assert(page.apply_draft() == ManualApplyResult::applied_rtc_write_failed);
  snapshot = page.date_time_snapshot();
  assert(snapshot.has_apply_result && !snapshot.draft_modified);
  assert(snapshot.current.year == 2025U && snapshot.current.minute == 7U &&
         snapshot.current.second == 55U);

  virtual_time.fail_rtc_write = false;
  assert(page.set_draft({2026U, 1U, 1U, 0U, 1U, 20U}));
  assert(page.apply_draft() == ManualApplyResult::applied);
  assert(page.date_time_snapshot().source == hmi::time::TimeSource::manual);
  page.destroy();

  assert(page.create(screen, context));
  assert(page.date_time_snapshot().current.year == 2026U);
  page.destroy();
  lv_obj_del(screen);
  theme.reset();
  return 0;
}
