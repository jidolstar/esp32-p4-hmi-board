#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

#include <lvgl.h>

#include "app.hpp"
#include "hmi/audio/playback_port.hpp"
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
  for (std::uint32_t index = 0U; index < count; ++index) {
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

void press_sound_row(const char* title) {
  lv_obj_t* label = find_label(lv_scr_act(), title);
  assert(label != nullptr);
  lv_obj_t* column = lv_obj_get_parent(label);
  assert(column != nullptr);
  lv_obj_t* row = lv_obj_get_parent(column);
  assert(row != nullptr);
  lv_event_send(row, LV_EVENT_SHORT_CLICKED, nullptr);
}

struct FakePlayback {
  hmi::audio::PlaybackSnapshot snapshot{hmi::audio::PlaybackState::idle,
                                        hmi::audio::PlaybackError::none};
  unsigned play_count = 0U;
  unsigned pause_count = 0U;
  unsigned resume_count = 0U;
  unsigned stop_count = 0U;
  bool fail_next = false;
};

bool play(void* context, const char*) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.play_count;
  ++fake.snapshot.revision;
  fake.snapshot.position_ms = 0U;
  fake.snapshot.duration_ms = 2208U;
  if (fake.fail_next) {
    fake.fail_next = false;
    fake.snapshot.state = hmi::audio::PlaybackState::error;
    fake.snapshot.error = hmi::audio::PlaybackError::file;
    return false;
  }
  fake.snapshot.state = hmi::audio::PlaybackState::playing;
  fake.snapshot.error = hmi::audio::PlaybackError::none;
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

void finish_operation(
    hmi::storage::StorageAccessController& controller,
    hmi::platform::windows::storage::VirtualStorageAccess& port,
    std::uint32_t& now_ms) {
  controller.tick(++now_ms);
  assert(port.complete_pending());
  controller.tick(++now_ms);
}

}  // namespace

int main() {
  using hmi::samples::suite::SampleSuiteApp;
  using hmi::samples::suite::SuiteView;
  using hmi::storage::StorageAccessState;

  lv_init();
  auto lvgl = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(lvgl->begin("wav sounds integration", false));

  FakePlayback fake;
  hmi::audio::AudioPlaybackPort audio_port = {
      &fake, play, stop, set_volume, probe, snapshot, pause, resume};
  hmi::platform::windows::storage::VirtualStorageAccess storage_port;
  hmi::storage::StorageAccessController storage(storage_port);
  std::uint32_t now_ms = 0U;
  storage.tick(now_ms);

  hmi::profiles::AppContext context;
  context.host_name = "windows-test";
  context.audio_playback = &audio_port;
  context.storage_access = &storage;
  auto app = std::make_unique<SampleSuiteApp>();
  assert(app->create(context));
  assert(find_label(lv_scr_act(), "등록 WAV") != nullptr);
  assert(app->request_page("wav-sounds"));
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kPage);
  assert(std::strcmp(app->snapshot().active_page_id, "wav-sounds") == 0);

  press_sound_row("조리 시작");
  assert(fake.play_count == 1U);
  fake.fail_next = true;
  press_sound_row("조리 완료");
  assert(fake.play_count == 2U);
  assert(find_label(lv_scr_act(), "등록된 WAV 파일을 찾을 수 없습니다.") !=
         nullptr);
  press_sound_row("뚜껑 열림");
  assert(fake.play_count == 3U);
  assert(find_label(lv_scr_act(), "재생 중") != nullptr);
  fake.snapshot.position_ms = 800U;
  press_button("일시정지");
  assert(fake.pause_count == 1U);
  assert(find_label(lv_scr_act(), "계속 재생") != nullptr);
  press_button("계속 재생");
  assert(fake.resume_count == 1U);
  assert(fake.snapshot.position_ms == 800U);

  press_button("영어");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Registered WAV") != nullptr);
  assert(fake.stop_count == 1U);
  press_button("Dark");
  assert(app->flush_pending_transition());
  assert(find_label(lv_scr_act(), "Light") != nullptr);
  assert(fake.stop_count == 2U);
  press_sound_row("Lid closed");
  assert(fake.play_count == 4U);

  assert(storage.request_pc_access(now_ms));
  app->refresh_storage_access_lock();
  assert(fake.stop_count == 3U);
  assert(app->snapshot().storage_lock.active);
  app->refresh_storage_access_lock();
  assert(fake.stop_count == 3U);
  finish_operation(storage, storage_port, now_ms);
  app->refresh_storage_access_lock();
  assert(storage.snapshot().state == StorageAccessState::waiting_for_host);
  assert(!app->request_dashboard());

  assert(storage.request_hmi_access(
      now_ms, hmi::storage::StorageReclaimReason::explicit_request));
  finish_operation(storage, storage_port, now_ms);
  app->refresh_storage_access_lock();
  assert(storage.snapshot().state == StorageAccessState::hmi_ready);
  assert(!app->snapshot().storage_lock.active);
  press_sound_row("Cooking complete");
  assert(fake.play_count == 5U);

  assert(app->request_dashboard());
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kDashboard);
  assert(fake.stop_count == 4U);
  app->destroy();
  return 0;
}
