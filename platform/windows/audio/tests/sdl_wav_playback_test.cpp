#include <cassert>

#include "audio/sdl_wav_playback.hpp"
#include "generated/wav_catalog.hpp"
#include "hmi/audio/playback.hpp"

int main() {
  hmi::platform::audio::SdlWavPlayback adapter("samples/suite/");
  hmi::audio::AudioPlayback playback;
  assert(adapter.valid());
  assert(playback.begin(adapter.port()));

  for (const auto& entry : hmi::samples::suite::generated::kWavCatalog) {
    hmi::audio::AudioFileInfo info;
    assert(playback.probe(entry.asset_path, info) ==
           hmi::audio::AudioProbeResult::ok);
    assert(info.file_bytes == entry.file_bytes);
    assert(info.sample_rate_hz == entry.sample_rate_hz);
    assert(info.duration_ms == entry.duration_ms);
    assert(info.channels == entry.channels);
    assert(info.bits_per_sample == entry.bits_per_sample);
  }

  assert(playback.play(
      hmi::samples::suite::generated::kWavCatalog.front().asset_path));
  const auto playing = playback.snapshot();
  assert(playing.state == hmi::audio::PlaybackState::playing ||
         playing.state == hmi::audio::PlaybackState::completed);
  assert(playing.duration_ms ==
         hmi::samples::suite::generated::kWavCatalog.front().duration_ms);
  if (playing.state == hmi::audio::PlaybackState::playing) {
    SDL_Delay(40U);
    assert(playback.pause());
    const auto paused = playback.snapshot();
    assert(paused.state == hmi::audio::PlaybackState::paused);
    SDL_Delay(40U);
    const auto still_paused = playback.snapshot();
    assert(still_paused.state == hmi::audio::PlaybackState::paused);
    assert(still_paused.position_ms == paused.position_ms);
    assert(playback.resume());
    const auto resumed = playback.snapshot();
    assert(resumed.state == hmi::audio::PlaybackState::playing ||
           resumed.state == hmi::audio::PlaybackState::completed);
    assert(resumed.position_ms >= paused.position_ms);
  }
  assert(playback.stop());
  const auto stopped = playback.snapshot();
  assert(stopped.state == hmi::audio::PlaybackState::stopped);
  assert(stopped.position_ms == 0U);
  hmi::audio::AudioFileInfo invalid;
  assert(playback.probe("../outside.wav", invalid) ==
         hmi::audio::AudioProbeResult::invalid);
  return 0;
}
