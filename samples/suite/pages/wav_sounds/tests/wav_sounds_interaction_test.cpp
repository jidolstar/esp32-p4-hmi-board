#include <cassert>
#include <cstring>
#include <cstdint>
#include <memory>
#include <string>

#include <lvgl.h>

#include "hmi/audio/playback_port.hpp"
#include "pages/wav_sounds/wav_sounds_page.hpp"
#include "pointer_test_driver.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

lv_obj_t* find_label(lv_obj_t* root, const char* text) {
  if (root == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(root, &lv_label_class) &&
      std::string(lv_label_get_text(root)) == text) {
    return root;
  }
  const std::uint32_t count = lv_obj_get_child_cnt(root);
  for (std::uint32_t index = 0U; index < count; ++index) {
    if (lv_obj_t* found = find_label(lv_obj_get_child(root, index), text)) {
      return found;
    }
  }
  return nullptr;
}

void assert_touch_target(lv_obj_t* object) {
  assert(object != nullptr);
  lv_area_t area{};
  lv_obj_get_coords(object, &area);
  assert(area.x1 >= 0 && area.y1 >= 0);
  assert(area.x2 < 1024 && area.y2 < 600);
  assert(area.x2 - area.x1 + 1 >= 48);
  assert(area.y2 - area.y1 + 1 >= 48);
}

struct FakePlayback {
  hmi::audio::PlaybackSnapshot snapshot{hmi::audio::PlaybackState::idle,
                                        hmi::audio::PlaybackError::none};
  unsigned play_count = 0U;
  unsigned pause_count = 0U;
  unsigned resume_count = 0U;
  unsigned stop_count = 0U;
};

bool play(void* context, const char*) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.play_count;
  ++fake.snapshot.revision;
  fake.snapshot.state = hmi::audio::PlaybackState::playing;
  fake.snapshot.error = hmi::audio::PlaybackError::none;
  fake.snapshot.duration_ms = 2208U;
  return true;
}

bool pause(void* context) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.pause_count;
  ++fake.snapshot.revision;
  fake.snapshot.state = hmi::audio::PlaybackState::paused;
  return true;
}

bool resume(void* context) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.resume_count;
  ++fake.snapshot.revision;
  fake.snapshot.state = hmi::audio::PlaybackState::playing;
  return true;
}

bool stop(void* context) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.stop_count;
  ++fake.snapshot.revision;
  fake.snapshot.state = hmi::audio::PlaybackState::stopped;
  fake.snapshot.position_ms = 0U;
  fake.snapshot.duration_ms = 0U;
  return true;
}

bool set_volume(void*, std::uint8_t value) noexcept { return value <= 100U; }
hmi::audio::AudioProbeResult probe(
    void*, const char*, hmi::audio::AudioFileInfo& output) noexcept {
  output = {212046U, 48000U, 2208U, 1U, 16U};
  return hmi::audio::AudioProbeResult::ok;
}
hmi::audio::PlaybackSnapshot snapshot(void* context) noexcept {
  return static_cast<FakePlayback*>(context)->snapshot;
}

struct Requests {
  unsigned back = 0U;
  unsigned theme = 0U;
  unsigned locale = 0U;
};

void back(void* context) noexcept { ++static_cast<Requests*>(context)->back; }
void theme(void* context) noexcept { ++static_cast<Requests*>(context)->theme; }
bool locale(void* context, hmi::i18n::Locale) noexcept {
  ++static_cast<Requests*>(context)->locale;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("wav sounds interaction", false));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  FakePlayback fake;
  hmi::audio::AudioPlaybackPort audio_port = {
      &fake, play, stop, set_volume, probe, snapshot, pause, resume};
  hmi::ui::UiTheme ui_theme;
  assert(ui_theme.initialize(hmi::ui::ThemeMode::kLight));
  Requests requests;
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &back;
  context.request_theme_toggle = &theme;
  context.set_locale = &locale;
  context.audio_playback = &audio_port;
  context.theme = &ui_theme;
  context.user_data = &requests;

  hmi::samples::suite::WavSoundsPage page;
  assert(page.create(screen, context));
  lv_obj_update_layout(screen);
  assert(find_label(screen, "등록 WAV") != nullptr);
  assert(find_label(screen, "조리 시작") != nullptr);
  assert(find_label(screen, "재생 가능한 WAV") != nullptr);
  assert(find_label(screen, "--:--") != nullptr);
  const char* screenshot_base =
      argc == 3 && std::strcmp(argv[1], "--screenshot") == 0 ? argv[2]
                                                               : nullptr;
  if (screenshot_base != nullptr) {
    lv_obj_invalidate(screen);
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(
        (std::string(screenshot_base) + ".initial.bmp").c_str()));
  }

  lv_obj_t* first_title = find_label(screen, "조리 시작");
  assert(first_title != nullptr);
  lv_obj_t* first_row = lv_obj_get_parent(lv_obj_get_parent(first_title));
  assert(first_row != nullptr);
  assert_touch_target(first_row);
  // 객체에 event를 직접 주입하지 않고 실제 pointer 좌표로 title 중앙을 누른다.
  // 투명한 text column이 row의 hit target을 가로채는 회귀를 이 경로가 검출한다.
  lv_area_t first_title_area{};
  lv_obj_get_coords(first_title, &first_title_area);
  hmi::platform::windows::testing::click_pointer(
      *lvgl, (first_title_area.x1 + first_title_area.x2) / 2,
      (first_title_area.y1 + first_title_area.y2) / 2);
  assert(fake.play_count == 1U);
  assert(page.playback_snapshot().selected_index == 0U);
  assert(find_label(screen, "재생 중") != nullptr);
  assert(find_label(screen, "00:02") != nullptr);
  fake.snapshot.position_ms = 700U;
  page.refresh();
  lv_obj_t* pause_label = find_label(screen, "일시정지");
  assert(pause_label != nullptr);
  lv_obj_t* primary_button = lv_obj_get_parent(pause_label);
  assert_touch_target(primary_button);
  lv_area_t primary_area{};
  lv_obj_get_coords(primary_button, &primary_area);
  hmi::platform::windows::testing::click_pointer(
      *lvgl, (primary_area.x1 + primary_area.x2) / 2,
      (primary_area.y1 + primary_area.y2) / 2);
  assert(fake.pause_count == 1U);
  assert(page.playback_snapshot().elapsed_ms == 700U);
  assert(find_label(screen, "계속 재생") != nullptr);
  hmi::platform::windows::testing::click_pointer(
      *lvgl, (primary_area.x1 + primary_area.x2) / 2,
      (primary_area.y1 + primary_area.y2) / 2);
  assert(fake.resume_count == 1U);
  assert(page.playback_snapshot().elapsed_ms == 700U);
  if (screenshot_base != nullptr) {
    fake.snapshot.position_ms = 1104U;
    page.refresh();
    lv_obj_invalidate(screen);
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(
        (std::string(screenshot_base) + ".playing.bmp").c_str()));
  }

  lv_obj_t* stop_label = find_label(screen, "중지");
  assert(stop_label != nullptr);
  assert_touch_target(lv_obj_get_parent(stop_label));
  lv_event_send(lv_obj_get_parent(stop_label), LV_EVENT_SHORT_CLICKED, nullptr);
  assert(fake.stop_count == 1U);
  assert(find_label(screen, "재생을 중지했습니다.") != nullptr);

  lv_obj_t* play_label = find_label(screen, "재생");
  assert(play_label != nullptr);
  assert_touch_target(lv_obj_get_parent(play_label));
  lv_event_send(lv_obj_get_parent(play_label), LV_EVENT_SHORT_CLICKED, nullptr);
  assert(fake.play_count == 2U);
  if (screenshot_base != nullptr) {
    fake.snapshot.state = hmi::audio::PlaybackState::completed;
    fake.snapshot.position_ms = fake.snapshot.duration_ms;
    page.refresh();
    lv_obj_invalidate(screen);
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(
        (std::string(screenshot_base) + ".completed.bmp").c_str()));
  }

  lv_obj_t* english = find_label(screen, "영어");
  lv_obj_t* dark = find_label(screen, "어둡게");
  lv_obj_t* back_label = find_label(screen, "뒤로");
  assert(english != nullptr && dark != nullptr && back_label != nullptr);
  lv_event_send(lv_obj_get_parent(english), LV_EVENT_SHORT_CLICKED, nullptr);
  lv_event_send(lv_obj_get_parent(dark), LV_EVENT_SHORT_CLICKED, nullptr);
  lv_event_send(lv_obj_get_parent(back_label), LV_EVENT_SHORT_CLICKED, nullptr);
  assert(requests.locale == 1U && requests.theme == 1U && requests.back == 1U);

  page.destroy();
  assert(ui_theme.set_mode(hmi::ui::ThemeMode::kDark));
  context.locale = hmi::i18n::Locale::en_us;
  assert(page.create(screen, context));
  assert(find_label(screen, "Registered WAV") != nullptr);
  assert(find_label(screen, "Supported WAV files") != nullptr);
  assert(page.play_index(4U));
  fake.snapshot.position_ms = 1776U;
  page.refresh();
  if (screenshot_base != nullptr) {
    lv_obj_invalidate(screen);
    lv_refr_now(nullptr);
    assert(lvgl->save_screenshot(
        (std::string(screenshot_base) + ".dark-en.bmp").c_str()));
  }
  page.destroy();
  const unsigned stopped_after_destroy = fake.stop_count;
  lv_timer_handler();
  assert(fake.stop_count == stopped_after_destroy);

  lv_obj_del(screen);
  ui_theme.reset();
  return 0;
}
