#include <cassert>
#include <cstdint>
#include <limits>

#include "hmi/audio/playback.hpp"

namespace {

struct FakePlayback {
  hmi::audio::PlaybackSnapshot snapshot{};
  unsigned play_count = 0U;
  unsigned pause_count = 0U;
  unsigned resume_count = 0U;
  unsigned stop_count = 0U;
  std::uint8_t volume = 100U;
};

bool play(void* context, const char* path) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  ++fake.play_count;
  fake.snapshot.state = hmi::audio::PlaybackState::playing;
  ++fake.snapshot.revision;
  return true;
}

bool pause(void* context) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.pause_count;
  fake.snapshot.state = hmi::audio::PlaybackState::paused;
  ++fake.snapshot.revision;
  return true;
}

bool resume(void* context) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.resume_count;
  fake.snapshot.state = hmi::audio::PlaybackState::playing;
  ++fake.snapshot.revision;
  return true;
}

bool stop(void* context) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  ++fake.stop_count;
  fake.snapshot.state = hmi::audio::PlaybackState::stopped;
  fake.snapshot.position_ms = 0U;
  ++fake.snapshot.revision;
  return true;
}

bool set_volume(void* context, std::uint8_t value) noexcept {
  auto& fake = *static_cast<FakePlayback*>(context);
  fake.volume = value;
  fake.snapshot.volume_percent = value;
  return true;
}

hmi::audio::AudioProbeResult probe(void*, const char*,
                                   hmi::audio::AudioFileInfo& output) noexcept {
  output = {2048U, 48000U, 1000U, 1U, 16U};
  return hmi::audio::AudioProbeResult::ok;
}

hmi::audio::PlaybackSnapshot snapshot(void* context) noexcept {
  return static_cast<FakePlayback*>(context)->snapshot;
}

}  // namespace

int main() {
  using namespace hmi::audio;

  FakePlayback fake;
  AudioPlayback playback;
  assert(playback.begin(
      {&fake, play, stop, set_volume, probe, snapshot, pause, resume}));
  assert(playback.play("assets/sounds/notice.wav"));
  assert(fake.play_count == 1U);
  assert(playback.set_volume(35U));
  assert(fake.volume == 35U);
  assert(!playback.set_volume(101U));

  AudioFileInfo info;
  assert(playback.probe("assets/sounds/notice.wav", info) ==
         AudioProbeResult::ok);
  assert(info.duration_ms == 1000U);

  fake.snapshot.position_ms = 250U;
  fake.snapshot.duration_ms = 1000U;
  assert(playback.pause());
  assert(fake.pause_count == 1U);
  assert(playback.snapshot().position_ms == 250U);
  assert(playback.resume());
  assert(fake.resume_count == 1U);
  assert(playback.snapshot().position_ms == 250U);
  PlaybackProgress progress = playback.progress();
  assert(progress.duration_known);
  assert(progress.elapsed_ms == 250U);
  assert(progress.total_ms == 1000U);
  assert(progress.remaining_ms == 750U);
  assert(progress.permille == 250U);

  fake.snapshot.position_ms = 1200U;
  progress = playback.progress();
  assert(progress.elapsed_ms == 1000U);
  assert(progress.remaining_ms == 0U);
  assert(progress.permille == 1000U);

  fake.snapshot.position_ms = 0U;
  fake.snapshot.duration_ms = 0U;
  progress = playback.progress();
  assert(!progress.duration_known);
  assert(progress.elapsed_ms == 0U && progress.total_ms == 0U);
  assert(progress.remaining_ms == 0U && progress.permille == 0U);

  PlaybackSnapshot large;
  large.position_ms = std::numeric_limits<std::uint32_t>::max() - 1U;
  large.duration_ms = std::numeric_limits<std::uint32_t>::max();
  progress = AudioPlayback::progress_of(large);
  assert(progress.permille == 999U);

  assert(playback.stop());
  assert(fake.stop_count == 1U);
  playback.end();
  assert(fake.stop_count == 2U);
  assert(!playback.ready());
  return 0;
}

