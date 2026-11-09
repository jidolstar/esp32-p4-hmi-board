#include <cassert>
#include <cstring>
#include <memory>

#include <lvgl.h>

#include "app.hpp"
#include "sdl_lvgl_port.hpp"

int main() {
  lv_init();
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(port->begin("sample-suite navigation", false));

  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  hmi::profiles::AppContext context{"windows"};
  assert(app->create(context));
  assert(app->snapshot().view == hmi::samples::suite::SuiteView::kDashboard);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kLight);

  assert(app->request_theme_toggle());
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == hmi::samples::suite::SuiteView::kDashboard);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kDark);

  assert(app->request_page("ui-basics"));
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == hmi::samples::suite::SuiteView::kPage);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kDark);
  assert(app->request_theme_toggle());
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == hmi::samples::suite::SuiteView::kPage);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kLight);
  assert(app->request_dashboard());
  assert(app->flush_pending_transition());
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kLight);

  for (int iteration = 0; iteration < 3; ++iteration) {
    assert(app->request_page("ui-basics"));
    assert(app->flush_pending_transition());
    const auto page = app->snapshot();
    assert(page.view == hmi::samples::suite::SuiteView::kPage);
    assert(page.theme_mode == hmi::ui::ThemeMode::kLight);
    assert(page.active_page_id != nullptr);
    assert(std::strcmp(page.active_page_id, "ui-basics") == 0);

    assert(app->request_dashboard());
    assert(app->flush_pending_transition());
    const auto dashboard = app->snapshot();
    assert(dashboard.view == hmi::samples::suite::SuiteView::kDashboard);
    assert(dashboard.active_page_id == nullptr);
    assert(dashboard.theme_mode == hmi::ui::ThemeMode::kLight);
  }
  assert(!app->request_page("missing-page"));
  app->destroy();
  return 0;
}
