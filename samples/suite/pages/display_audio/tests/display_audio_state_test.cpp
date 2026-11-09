#include <cassert>
#include <cstdint>
#include <memory>

#include <lvgl.h>

#include "pages/display_audio/display_audio_page.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

struct FakeSettings {
  std::uint8_t brightness = 5U;
  std::uint8_t volume = 4U;
  unsigned preview_count = 0U;
  unsigned stop_count = 0U;
};

bool load(void* context, std::uint8_t& brightness, std::uint8_t& volume,
          bool& has_brightness, bool& has_volume) {
  auto& fake = *static_cast<FakeSettings*>(context);
  brightness = fake.brightness;
  volume = fake.volume;
  has_brightness = true;
  has_volume = true;
  return true;
}

bool save_brightness(void* context, std::uint8_t value) {
  static_cast<FakeSettings*>(context)->brightness = value;
  return true;
}

bool save_volume(void* context, std::uint8_t value) {
  static_cast<FakeSettings*>(context)->volume = value;
  return true;
}

bool apply_level(void*, std::uint8_t) { return true; }

bool play(void* context) {
  ++static_cast<FakeSettings*>(context)->preview_count;
  return true;
}

bool stop(void* context) {
  ++static_cast<FakeSettings*>(context)->stop_count;
  return true;
}

void ignore_back(void*) noexcept {}

void ignore_theme_toggle(void*) noexcept {}

bool accept_locale(void*, hmi::i18n::Locale) noexcept { return true; }

}  // namespace

int main() {
  using hmi::samples::suite::DisplayAudioPage;
  using hmi::samples::suite::SettingsPageStatus;
  using hmi::settings::LevelSettingsResult;

  lv_init();
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(port->begin("display-audio state", false));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  FakeSettings fake;
  hmi::ui::UiTheme theme;
  assert(theme.initialize());
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &ignore_back;
  context.request_theme_toggle = &ignore_theme_toggle;
  context.set_locale = &accept_locale;
  context.theme = &theme;
  const hmi::settings::LevelSettingsPort settings_port = {
      &fake, load, save_brightness, save_volume, apply_level, apply_level, play,
      stop};
  context.level_settings = &settings_port;

  DisplayAudioPage page;
  assert(page.create(screen, context));
  assert(page.snapshot().brightness == 5U);
  assert(page.snapshot().volume == 4U);
  assert(page.snapshot().settings_status == SettingsPageStatus::kReady);
  assert(page.set_brightness(7U) == LevelSettingsResult::ok);
  assert(page.set_volume(0U) == LevelSettingsResult::ok);
  assert(fake.stop_count == 1U);
  assert(page.set_volume(6U) == LevelSettingsResult::ok);
  assert(page.play_preview() == LevelSettingsResult::ok);
  assert(fake.brightness == 7U && fake.volume == 6U);
  assert(fake.preview_count == 2U);
  page.destroy();
  assert(fake.stop_count == 2U);

  lv_obj_del(screen);
  theme.reset();
  return 0;
}
