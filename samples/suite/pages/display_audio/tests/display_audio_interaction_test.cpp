#include <cassert>
#include <cstring>
#include <memory>

#include <lvgl.h>

#include "app.hpp"
#include "audio/sdl_wav_playback.hpp"
#include "hmi/audio/playback_port.hpp"
#include "settings/memory_level_settings.hpp"
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
  for (std::uint32_t index = 0; index < count; ++index) {
    if (lv_obj_t* found = find_label(lv_obj_get_child(parent, index), text)) {
      return found;
    }
  }
  return nullptr;
}

void collect_sliders(lv_obj_t* parent, lv_obj_t** sliders,
                     std::size_t& count) {
  if (parent == nullptr || count >= 2U) {
    return;
  }
  if (lv_obj_check_type(parent, &lv_slider_class)) {
    sliders[count++] = parent;
  }
  const std::uint32_t child_count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0; index < child_count; ++index) {
    collect_sliders(lv_obj_get_child(parent, index), sliders, count);
  }
}

void click_label(lv_obj_t* screen, const char* text) {
  lv_obj_t* label = find_label(screen, text);
  assert(label != nullptr);
  lv_obj_t* button = lv_obj_get_parent(label);
  assert(button != nullptr);
  const lv_res_t result =
      lv_event_send(button, LV_EVENT_SHORT_CLICKED, nullptr);
  assert(result == LV_RES_OK || result == LV_RES_INV);
}

}  // namespace

int main(int argc, char** argv) {
  using hmi::samples::suite::SettingsPageStatus;
  using hmi::samples::suite::SuiteView;

  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("display-audio interaction", false));
  hmi::platform::audio::SdlWavPlayback audio("A:samples/suite/");
  hmi::audio::AudioPlaybackPort audio_port = audio.port();
  hmi::platform::settings::MemoryLevelSettings settings(*lvgl, audio_port);
  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  hmi::profiles::AppContext context;
  context.host_name = "windows";
  context.level_settings = settings.port();
  context.audio_playback = &audio_port;
  assert(app->create(context));
  assert(app->request_page("display-audio"));
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(find_label(lv_scr_act(), "화면과 음량") != nullptr);
  assert(find_label(lv_scr_act(), "화면 밝기") != nullptr);
  assert(find_label(lv_scr_act(), "시스템 음량") != nullptr);
  assert(find_label(lv_scr_act(), "10 / 10") != nullptr);
  assert(find_label(lv_scr_act(), "한국어") != nullptr);
  assert(find_label(lv_scr_act(), "영어") != nullptr);
  assert(find_label(lv_scr_act(), "어둡게") != nullptr);
  assert(find_label(lv_scr_act(),
                    "밝기와 시스템 음량을 바로 조절하고 저장합니다.") ==
         nullptr);
  lv_obj_update_layout(lv_scr_act());
  if (argc == 3 && std::strcmp(argv[1], "--screenshot-dark") == 0) {
    click_label(lv_scr_act(), "영어");
    assert(app->flush_pending_transition());
    click_label(lv_scr_act(), "Dark");
    assert(app->flush_pending_transition());
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(argv[2]));
    app->destroy();
    return 0;
  }

  lv_obj_t* sliders[2] = {nullptr, nullptr};
  std::size_t slider_count = 0U;
  collect_sliders(lv_scr_act(), sliders, slider_count);
  assert(slider_count == 2U);
  for (lv_obj_t* slider : sliders) {
    lv_area_t area{};
    lv_obj_get_coords(slider, &area);
    assert(area.x1 >= 0 && area.y1 >= 0);
    assert(area.x2 < 1024 && area.y2 < 600);
  }
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(argv[2]));
  }
  lv_slider_set_value(sliders[0], 6, LV_ANIM_OFF);
  assert(lv_event_send(sliders[0], LV_EVENT_VALUE_CHANGED, nullptr) ==
         LV_RES_OK);
  lv_slider_set_value(sliders[1], 3, LV_ANIM_OFF);
  assert(lv_event_send(sliders[1], LV_EVENT_VALUE_CHANGED, nullptr) ==
         LV_RES_OK);
  assert(settings.applied_brightness() == 6U);
  assert(lvgl->brightness_level() == 6U);
  assert(settings.applied_volume() == 3U);
  assert(find_label(lv_scr_act(), "6 / 10") != nullptr);
  assert(find_label(lv_scr_act(), "3 / 10") != nullptr);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot-after") == 0) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(argv[2]));
  }
  assert(app->snapshot().page.settings_status == SettingsPageStatus::kSaved);

  const unsigned preview_before = settings.preview_count();
  click_label(lv_scr_act(), "미리 듣기");
  assert(settings.preview_count() == preview_before + 1U);
  click_label(lv_scr_act(), "영어");
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(app->snapshot().locale == hmi::i18n::Locale::en_us);
  assert(find_label(lv_scr_act(), "Display & Audio") != nullptr);
  assert(find_label(lv_scr_act(), "Display brightness") != nullptr);
  assert(find_label(lv_scr_act(), "System volume") != nullptr);
  assert(find_label(lv_scr_act(), "Play Preview") != nullptr);
  assert(find_label(lv_scr_act(),
                    "Adjust and save display brightness and system volume.") ==
         nullptr);
  click_label(lv_scr_act(), "Dark");
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(app->snapshot().theme_mode == hmi::ui::ThemeMode::kDark);
  assert(find_label(lv_scr_act(), "Light") != nullptr);
  click_label(lv_scr_act(), "Back");
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kDashboard);
  app->destroy();
  return 0;
}
