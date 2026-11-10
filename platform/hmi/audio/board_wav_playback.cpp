#include "audio/board_wav_playback.hpp"

#include <cstdio>
#include <cstring>

#include "board.hpp"

namespace hmi::platform::audio {
namespace {

hmi::audio::PlaybackState map_state(
    hmi::boards::jc1060p470c::AudioState state) noexcept {
  using Source = hmi::boards::jc1060p470c::AudioState;
  using Target = hmi::audio::PlaybackState;
  switch (state) {
    case Source::idle:
      return Target::idle;
    case Source::starting:
      return Target::starting;
    case Source::playing:
      return Target::playing;
    case Source::pausing:
      return Target::pausing;
    case Source::paused:
      return Target::paused;
    case Source::resuming:
      return Target::resuming;
    case Source::completed:
      return Target::completed;
    case Source::stopped:
      return Target::stopped;
    case Source::error:
      return Target::error;
  }
  return Target::error;
}

hmi::audio::PlaybackError map_error(
    hmi::boards::jc1060p470c::AudioError error) noexcept {
  using Source = hmi::boards::jc1060p470c::AudioError;
  using Target = hmi::audio::PlaybackError;
  switch (error) {
    case Source::none:
      return Target::none;
    case Source::unavailable:
      return Target::unavailable;
    case Source::path:
      return Target::path;
    case Source::file:
      return Target::file;
    case Source::format:
      return Target::unsupported;
    case Source::truncated:
      return Target::truncated;
    case Source::output:
      return Target::output;
    case Source::read:
      return Target::read;
  }
  return Target::read;
}

hmi::audio::AudioProbeResult map_probe(
    hmi::boards::jc1060p470c::AudioProbeResult result) noexcept {
  using Source = hmi::boards::jc1060p470c::AudioProbeResult;
  using Target = hmi::audio::AudioProbeResult;
  switch (result) {
    case Source::ok:
      return Target::ok;
    case Source::unavailable:
      return Target::unavailable;
    case Source::file:
      return Target::file;
    case Source::invalid:
      return Target::invalid;
    case Source::truncated:
      return Target::truncated;
    case Source::unsupported:
      return Target::unsupported;
  }
  return Target::invalid;
}

bool safe_relative_path(const char* path) noexcept {
  return path != nullptr && path[0] != '\0' && path[0] != '/' &&
         std::strchr(path, '\\') == nullptr && std::strstr(path, "..") == nullptr;
}

}  // namespace

BoardWavPlayback::BoardWavPlayback(
    hmi::boards::jc1060p470c::Board& board,
    const char* asset_storage_root) noexcept
    : board_(board) {
  if (asset_storage_root == nullptr || asset_storage_root[0] != '/') {
    return;
  }
  const int written = std::snprintf(root_.data(), root_.size(), "%s/",
                                    asset_storage_root);
  valid_ = written > 0 && static_cast<std::size_t>(written) < root_.size();
}

hmi::audio::AudioPlaybackPort BoardWavPlayback::port() noexcept {
  return valid_ ? hmi::audio::AudioPlaybackPort{this, play, stop, set_volume,
                                                probe, snapshot, pause, resume}
                : hmi::audio::AudioPlaybackPort{};
}

bool BoardWavPlayback::resolve(const char* relative_path,
                               std::array<char, 384U>& output) const noexcept {
  if (!valid_ || !safe_relative_path(relative_path)) {
    return false;
  }
  const int written = std::snprintf(output.data(), output.size(), "%s%s",
                                    root_.data(), relative_path);
  return written > 0 && static_cast<std::size_t>(written) < output.size();
}

bool BoardWavPlayback::play(void* context, const char* relative_path) noexcept {
  auto& self = *static_cast<BoardWavPlayback*>(context);
  std::array<char, 384U> path{};
  return self.resolve(relative_path, path) &&
         self.board_.audio().play(path.data());
}

bool BoardWavPlayback::stop(void* context) noexcept {
  return static_cast<BoardWavPlayback*>(context)->board_.audio().stop();
}

bool BoardWavPlayback::pause(void* context) noexcept {
  return static_cast<BoardWavPlayback*>(context)->board_.audio().pause();
}

bool BoardWavPlayback::resume(void* context) noexcept {
  return static_cast<BoardWavPlayback*>(context)->board_.audio().resume();
}

bool BoardWavPlayback::set_volume(void* context, std::uint8_t value) noexcept {
  return value <= 100U &&
         static_cast<BoardWavPlayback*>(context)->board_.audio().set_volume(value);
}

hmi::audio::AudioProbeResult BoardWavPlayback::probe(
    void* context, const char* relative_path,
    hmi::audio::AudioFileInfo& output) noexcept {
  output = {};
  auto& self = *static_cast<BoardWavPlayback*>(context);
  std::array<char, 384U> path{};
  if (!self.resolve(relative_path, path)) {
    return hmi::audio::AudioProbeResult::invalid;
  }
  hmi::boards::jc1060p470c::AudioFileInfo source;
  const auto result = self.board_.audio().probe(path.data(), source);
  if (result == hmi::boards::jc1060p470c::AudioProbeResult::ok) {
    output = {source.file_bytes, source.sample_rate, source.duration_ms,
              source.channels, source.bits_per_sample};
  }
  return map_probe(result);
}

hmi::audio::PlaybackSnapshot BoardWavPlayback::snapshot(void* context) noexcept {
  const auto source =
      static_cast<BoardWavPlayback*>(context)->board_.audio().status();
  return {map_state(source.state), map_error(source.error), source.position_ms,
          source.duration_ms, source.revision, source.volume};
}

}  // namespace hmi::platform::audio

