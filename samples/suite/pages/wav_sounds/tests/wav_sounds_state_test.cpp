#include <cassert>
#include <cstdint>
#include <memory>

#include <lvgl.h>

#include "hmi/audio/playback_port.hpp"
#include "pages/wav_sounds/wav_sounds_page.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

struct FakePlayback {
  hmi::audio::PlaybackSnapshot snapshot{hmi::audio::PlaybackState::idle,
                                        hmi::audio::PlaybackError::none};
  unsigned play_count = 0U;
  unsigned pause_count = 0U;
  unsigned resume_count = 0U;
  unsigned stop_count = 0U;
  bool fail_next = false;
};

bool play(void* context, const char* path) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.play_count;
  ++fake.snapshot.revision;
  fake.snapshot.position_ms = 0U;
  if (fake.fail_next || path == nullptr) {
    fake.fail_next = false;
    fake.snapshot.state = hmi::audio::PlaybackState::error;
    fake.snapshot.error = hmi::audio::PlaybackError::unsupported;
    fake.snapshot.duration_ms = 0U;
    return false;
  }
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
  fake.snapshot.error = hmi::audio::PlaybackError::none;
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

void ignore(void*) noexcept {}
bool accept_locale(void*, hmi::i18n::Locale) noexcept { return true; }

}  // namespace

int main() {
  using hmi::audio::PlaybackError;
  using hmi::audio::PlaybackState;
  using hmi::samples::suite::WavSoundsPage;

  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("wav sounds state", false));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  FakePlayback fake;
  hmi::audio::AudioPlaybackPort audio_port = {
      &fake, play, stop, set_volume, probe, snapshot, pause, resume};
  hmi::ui::UiTheme theme;
  assert(theme.initialize());
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &ignore;
  context.request_theme_toggle = &ignore;
  context.set_locale = &accept_locale;
  context.audio_playback = &audio_port;
  context.theme = &theme;

  WavSoundsPage page;
  assert(page.create(screen, context));
  auto value = page.playback_snapshot();
  assert(value.selected_index == 0xFFU);
  assert(!value.primary_enabled && !value.stop_enabled);

  assert(page.play_index(0U));
  value = page.playback_snapshot();
  assert(fake.play_count == 1U);
  assert(value.selected_index == 0U);
  assert(value.state == PlaybackState::playing);
  assert(value.total_ms == 2208U && value.remaining_ms == 2208U);
  assert(value.stop_enabled);

  fake.snapshot.position_ms = 1104U;
  page.refresh();
  value = page.playback_snapshot();
  assert(value.elapsed_ms == 1104U);
  assert(value.remaining_ms == 1104U);
  assert(value.progress_permille == 500U);
  assert(page.pause());
  value = page.playback_snapshot();
  assert(fake.pause_count == 1U && value.state == PlaybackState::paused);
  assert(value.elapsed_ms == 1104U && value.progress_permille == 500U);
  assert(value.primary_enabled && value.stop_enabled);
  assert(page.resume());
  value = page.playback_snapshot();
  assert(fake.resume_count == 1U && value.state == PlaybackState::playing);
  assert(value.elapsed_ms == 1104U && value.progress_permille == 500U);

  fake.snapshot.state = PlaybackState::completed;
  fake.snapshot.position_ms = fake.snapshot.duration_ms;
  page.refresh();
  value = page.playback_snapshot();
  assert(value.progress_permille == 1000U);
  assert(value.remaining_ms == 0U);
  assert(value.primary_enabled && !value.stop_enabled);
  assert(page.play_index(0U));
  assert(fake.play_count == 2U);

  assert(page.stop());
  value = page.playback_snapshot();
  assert(value.state == PlaybackState::stopped);
  assert(value.elapsed_ms == 0U && value.progress_permille == 0U);
  assert(!value.stop_enabled);

  fake.fail_next = true;
  assert(!page.play_index(1U));
  value = page.playback_snapshot();
  assert(value.selected_index == 1U);
  assert(value.state == PlaybackState::error);
  assert(value.error == PlaybackError::unsupported);
  assert(value.primary_enabled && !value.stop_enabled);

  assert(page.play_index(2U));
  assert(page.playback_snapshot().selected_index == 2U);
  assert(fake.play_count == 4U);
  page.destroy();
  assert(fake.stop_count == 2U);
  lv_timer_handler();

  lv_obj_del(screen);
  theme.reset();
  return 0;
}
