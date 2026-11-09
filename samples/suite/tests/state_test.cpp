#include <cassert>
#include <memory>

#include <lvgl.h>

#include "pages/ui_basics/ui_basics_page.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

void ignore_back(void *) noexcept {}
void ignore_theme_toggle(void *) noexcept {}
bool accept_locale(void *, hmi::i18n::Locale) noexcept { return true; }

}  // namespace

int main() {
  using hmi::samples::suite::OverlayState;
  using hmi::samples::suite::PageAction;

  lv_init();
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(port->begin("ui-basics state", false));
  lv_obj_t *screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  hmi::samples::suite::UiBasicsPage page;
  hmi::ui::UiTheme theme;
  assert(theme.initialize());
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &ignore_back;
  context.request_theme_toggle = &ignore_theme_toggle;
  context.set_locale = &accept_locale;
  context.theme = &theme;
  assert(page.create(screen, context));

  page.record_press();
  page.record_long_press();
  auto state = page.snapshot();
  assert(state.press_count == 1);
  assert(state.long_press_count == 1);
  assert(state.last_action == PageAction::kLongPress);

  assert(page.show_toast());
  state = page.snapshot();
  assert(state.overlay == OverlayState::kToast);
  assert(state.last_action == PageAction::kToastShown);
  page.expire_toast();
  state = page.snapshot();
  assert(state.overlay == OverlayState::kNone);
  assert(state.last_action == PageAction::kToastExpired);

  assert(page.open_dialog());
  page.resolve_dialog(false);
  assert(page.snapshot().last_action == PageAction::kDialogCancelled);
  assert(page.open_dialog());
  page.resolve_dialog(true);
  assert(page.snapshot().last_action == PageAction::kDialogConfirmed);

  page.destroy();
  lv_obj_del(screen);
  theme.reset();
  return 0;
}
