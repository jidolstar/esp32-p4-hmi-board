#pragma once

#include <array>
#include <cstdint>

#include <SDL.h>

#include "hmi/audio/playback_port.hpp"

namespace hmi::platform::audio {

/**
 * profile asset root의 등록 WAV 하나를 SDL queued audio로 재생하는 Windows adapter다.
 *
 * SDL device와 queued PCM은 이 객체가 소유한다. 반환한 port는 이 객체보다 오래 보관하면
 * 안 되며 상대 asset path만 허용한다.
 */
class SdlWavPlayback {
 public:
  explicit SdlWavPlayback(const char* asset_root) noexcept;
  ~SdlWavPlayback();
  SdlWavPlayback(const SdlWavPlayback&) = delete;
  SdlWavPlayback& operator=(const SdlWavPlayback&) = delete;

  [[nodiscard]] hmi::audio::AudioPlaybackPort port() noexcept;
  [[nodiscard]] bool valid() const noexcept { return root_valid_; }

 private:
  static bool play(void*, const char*) noexcept;
  static bool pause(void*) noexcept;
  static bool resume(void*) noexcept;
  static bool stop(void*) noexcept;
  static bool set_volume(void*, std::uint8_t) noexcept;
  static hmi::audio::AudioProbeResult probe(
      void*, const char*, hmi::audio::AudioFileInfo&) noexcept;
  static hmi::audio::PlaybackSnapshot snapshot(void*) noexcept;

  bool resolve(const char* relative_path, std::array<char, 384U>& output) const
      noexcept;
  bool open_audio(const SDL_AudioSpec& source_spec) noexcept;
  bool play_file(const char* relative_path) noexcept;
  bool pause_file() noexcept;
  bool resume_file() noexcept;
  bool stop_file() noexcept;
  hmi::audio::AudioProbeResult probe_file(
      const char* relative_path, hmi::audio::AudioFileInfo& output) noexcept;
  hmi::audio::PlaybackSnapshot refresh() noexcept;

  std::array<char, 320U> root_{};
  SDL_AudioDeviceID audio_device_ = 0U;
  SDL_AudioSpec audio_spec_{};
  hmi::audio::PlaybackState state_ = hmi::audio::PlaybackState::unavailable;
  hmi::audio::PlaybackError error_ = hmi::audio::PlaybackError::unavailable;
  std::uint32_t source_bytes_ = 0U;
  std::uint32_t bytes_per_second_ = 0U;
  std::uint32_t duration_ms_ = 0U;
  std::uint32_t position_ms_ = 0U;
  std::uint32_t revision_ = 0U;
  std::uint8_t volume_percent_ = 100U;
  bool audio_subsystem_owned_ = false;
  bool root_valid_ = false;
};

}  // namespace hmi::platform::audio

