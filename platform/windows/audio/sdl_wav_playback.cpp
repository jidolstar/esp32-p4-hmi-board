#include "audio/sdl_wav_playback.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace hmi::platform::audio {
namespace {

constexpr std::uint64_t kMaximumWavBytes = 1024U * 1024U;

bool same_audio_format(const SDL_AudioSpec& left,
                       const SDL_AudioSpec& right) noexcept {
  return left.freq == right.freq && left.format == right.format &&
         left.channels == right.channels;
}

bool safe_relative_path(const char* path) noexcept {
  return path != nullptr && path[0] != '\0' && path[0] != '/' &&
         std::strchr(path, '\\') == nullptr && std::strstr(path, "..") == nullptr;
}

std::uint16_t bits_per_sample(SDL_AudioFormat format) noexcept {
  return static_cast<std::uint16_t>(SDL_AUDIO_BITSIZE(format));
}

}  // namespace

SdlWavPlayback::SdlWavPlayback(const char* asset_root) noexcept {
  if (asset_root == nullptr || asset_root[0] == '\0') {
    return;
  }
  // LVGL stdio driver의 `A:` prefix는 SDL_LoadWAV가 이해하지 못하므로 제거한다.
  const char* filesystem_root =
      asset_root[0] != '\0' && asset_root[1] == ':' ? asset_root + 2 : asset_root;
  const int written =
      std::snprintf(root_.data(), root_.size(), "%s", filesystem_root);
  root_valid_ =
      written > 0 && static_cast<std::size_t>(written) < root_.size();
  if (root_valid_) {
    state_ = hmi::audio::PlaybackState::idle;
    error_ = hmi::audio::PlaybackError::none;
  }
}

SdlWavPlayback::~SdlWavPlayback() {
  stop_file();
  if (audio_device_ != 0U) {
    SDL_CloseAudioDevice(audio_device_);
  }
  if (audio_subsystem_owned_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

hmi::audio::AudioPlaybackPort SdlWavPlayback::port() noexcept {
  return root_valid_
             ? hmi::audio::AudioPlaybackPort{this, play, stop, set_volume,
                                             probe, snapshot, pause, resume}
             : hmi::audio::AudioPlaybackPort{};
}

bool SdlWavPlayback::resolve(const char* relative_path,
                             std::array<char, 384U>& output) const noexcept {
  if (!root_valid_ || !safe_relative_path(relative_path)) {
    return false;
  }
  const int written = std::snprintf(output.data(), output.size(), "%s%s",
                                    root_.data(), relative_path);
  return written > 0 && static_cast<std::size_t>(written) < output.size();
}

bool SdlWavPlayback::open_audio(const SDL_AudioSpec& source_spec) noexcept {
  if (audio_device_ != 0U && same_audio_format(audio_spec_, source_spec)) {
    return true;
  }
  if (audio_device_ != 0U) {
    SDL_CloseAudioDevice(audio_device_);
    audio_device_ = 0U;
  }
  if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      return false;
    }
    audio_subsystem_owned_ = true;
  }
  audio_spec_ = source_spec;
  audio_spec_.callback = nullptr;
  audio_spec_.userdata = nullptr;
  audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &audio_spec_, nullptr, 0);
  return audio_device_ != 0U;
}

bool SdlWavPlayback::play_file(const char* relative_path) noexcept {
  std::array<char, 384U> path{};
  if (!resolve(relative_path, path)) {
    state_ = hmi::audio::PlaybackState::error;
    error_ = hmi::audio::PlaybackError::path;
    return false;
  }
  struct _stat64 metadata {};
  if (_stat64(path.data(), &metadata) != 0 || metadata.st_size <= 0 ||
      static_cast<std::uint64_t>(metadata.st_size) > kMaximumWavBytes) {
    state_ = hmi::audio::PlaybackState::error;
    error_ = hmi::audio::PlaybackError::file;
    return false;
  }

  state_ = hmi::audio::PlaybackState::starting;
  error_ = hmi::audio::PlaybackError::none;
  SDL_AudioSpec source_spec{};
  std::uint8_t* source = nullptr;
  std::uint32_t source_size = 0U;
  if (SDL_LoadWAV(path.data(), &source_spec, &source, &source_size) == nullptr) {
    state_ = hmi::audio::PlaybackState::error;
    error_ = hmi::audio::PlaybackError::unsupported;
    return false;
  }
  if (source_spec.freq <= 0 || source_spec.channels == 0U ||
      !open_audio(source_spec)) {
    SDL_FreeWAV(source);
    state_ = hmi::audio::PlaybackState::error;
    error_ = hmi::audio::PlaybackError::output;
    return false;
  }

  std::vector<std::uint8_t> mixed(source_size, source_spec.silence);
  const int mix_volume = static_cast<int>(
      static_cast<unsigned>(volume_percent_) * SDL_MIX_MAXVOLUME / 100U);
  SDL_MixAudioFormat(mixed.data(), source, source_spec.format, source_size,
                     mix_volume);
  SDL_FreeWAV(source);
  SDL_ClearQueuedAudio(audio_device_);
  if (SDL_QueueAudio(audio_device_, mixed.data(), source_size) != 0) {
    state_ = hmi::audio::PlaybackState::error;
    error_ = hmi::audio::PlaybackError::output;
    return false;
  }
  SDL_PauseAudioDevice(audio_device_, 0);
  source_bytes_ = source_size;
  bytes_per_second_ = static_cast<std::uint32_t>(source_spec.freq) *
                      source_spec.channels * bits_per_sample(source_spec.format) /
                      8U;
  duration_ms_ = bytes_per_second_ == 0U
                     ? 0U
                     : static_cast<std::uint32_t>(
                           static_cast<std::uint64_t>(source_size) * 1000U /
                           bytes_per_second_);
  position_ms_ = 0U;
  ++revision_;
  state_ = hmi::audio::PlaybackState::playing;
  return true;
}

bool SdlWavPlayback::pause_file() noexcept {
  if (state_ != hmi::audio::PlaybackState::playing || audio_device_ == 0U) {
    return false;
  }
  refresh();
  if (state_ != hmi::audio::PlaybackState::playing) {
    return false;
  }
  SDL_PauseAudioDevice(audio_device_, 1);
  state_ = hmi::audio::PlaybackState::paused;
  ++revision_;
  return true;
}

bool SdlWavPlayback::resume_file() noexcept {
  if (state_ != hmi::audio::PlaybackState::paused || audio_device_ == 0U) {
    return false;
  }
  SDL_PauseAudioDevice(audio_device_, 0);
  state_ = hmi::audio::PlaybackState::playing;
  ++revision_;
  return true;
}

bool SdlWavPlayback::stop_file() noexcept {
  if (!root_valid_) {
    return false;
  }
  if (audio_device_ != 0U) {
    SDL_ClearQueuedAudio(audio_device_);
    SDL_PauseAudioDevice(audio_device_, 1);
  }
  ++revision_;
  state_ = hmi::audio::PlaybackState::stopped;
  error_ = hmi::audio::PlaybackError::none;
  source_bytes_ = 0U;
  bytes_per_second_ = 0U;
  duration_ms_ = 0U;
  position_ms_ = 0U;
  return true;
}

hmi::audio::AudioProbeResult SdlWavPlayback::probe_file(
    const char* relative_path, hmi::audio::AudioFileInfo& output) noexcept {
  output = {};
  std::array<char, 384U> path{};
  if (!resolve(relative_path, path)) {
    return hmi::audio::AudioProbeResult::invalid;
  }
  struct _stat64 metadata {};
  if (_stat64(path.data(), &metadata) != 0 || metadata.st_size <= 0 ||
      static_cast<std::uint64_t>(metadata.st_size) > kMaximumWavBytes) {
    return hmi::audio::AudioProbeResult::file;
  }
  SDL_AudioSpec source_spec{};
  std::uint8_t* source = nullptr;
  std::uint32_t source_size = 0U;
  if (SDL_LoadWAV(path.data(), &source_spec, &source, &source_size) == nullptr) {
    return hmi::audio::AudioProbeResult::unsupported;
  }
  const std::uint32_t bytes_per_second =
      static_cast<std::uint32_t>(source_spec.freq) * source_spec.channels *
      bits_per_sample(source_spec.format) / 8U;
  output.file_bytes = static_cast<std::uint64_t>(metadata.st_size);
  output.sample_rate_hz = static_cast<std::uint32_t>(source_spec.freq);
  output.duration_ms = bytes_per_second == 0U
                           ? 0U
                           : static_cast<std::uint32_t>(
                                 static_cast<std::uint64_t>(source_size) * 1000U /
                                 bytes_per_second);
  output.channels = source_spec.channels;
  output.bits_per_sample = bits_per_sample(source_spec.format);
  SDL_FreeWAV(source);
  return bytes_per_second == 0U ? hmi::audio::AudioProbeResult::invalid
                                : hmi::audio::AudioProbeResult::ok;
}

hmi::audio::PlaybackSnapshot SdlWavPlayback::refresh() noexcept {
  if ((state_ == hmi::audio::PlaybackState::playing ||
       state_ == hmi::audio::PlaybackState::paused) &&
      audio_device_ != 0U &&
      source_bytes_ > 0U && bytes_per_second_ > 0U) {
    const std::uint32_t queued =
        std::min<std::uint32_t>(SDL_GetQueuedAudioSize(audio_device_),
                                source_bytes_);
    const std::uint32_t consumed = source_bytes_ - queued;
    position_ms_ = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(consumed) * 1000U / bytes_per_second_);
    if (queued == 0U && state_ == hmi::audio::PlaybackState::playing) {
      position_ms_ = duration_ms_;
      state_ = hmi::audio::PlaybackState::completed;
    }
  } else if (state_ == hmi::audio::PlaybackState::completed) {
    position_ms_ = duration_ms_;
  }
  return {state_, error_, position_ms_, duration_ms_, revision_,
          volume_percent_};
}

bool SdlWavPlayback::play(void* context, const char* path) noexcept {
  return static_cast<SdlWavPlayback*>(context)->play_file(path);
}

bool SdlWavPlayback::pause(void* context) noexcept {
  return static_cast<SdlWavPlayback*>(context)->pause_file();
}

bool SdlWavPlayback::resume(void* context) noexcept {
  return static_cast<SdlWavPlayback*>(context)->resume_file();
}

bool SdlWavPlayback::stop(void* context) noexcept {
  return static_cast<SdlWavPlayback*>(context)->stop_file();
}

bool SdlWavPlayback::set_volume(void* context, std::uint8_t value) noexcept {
  if (value > 100U) {
    return false;
  }
  static_cast<SdlWavPlayback*>(context)->volume_percent_ = value;
  return true;
}

hmi::audio::AudioProbeResult SdlWavPlayback::probe(
    void* context, const char* path, hmi::audio::AudioFileInfo& output) noexcept {
  return static_cast<SdlWavPlayback*>(context)->probe_file(path, output);
}

hmi::audio::PlaybackSnapshot SdlWavPlayback::snapshot(void* context) noexcept {
  return static_cast<SdlWavPlayback*>(context)->refresh();
}

}  // namespace hmi::platform::audio

