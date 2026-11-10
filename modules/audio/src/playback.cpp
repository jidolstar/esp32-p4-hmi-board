#include "hmi/audio/playback.hpp"

namespace hmi::audio {

bool AudioPlayback::begin(AudioPlaybackPort port) noexcept {
  if (!port.valid() || port_.valid()) {
    return false;
  }
  port_ = port;
  return true;
}

void AudioPlayback::end() noexcept {
  if (port_.valid()) {
    port_.stop(port_.context);
  }
  port_ = {};
}

bool AudioPlayback::play(const char* relative_path) noexcept {
  return port_.valid() && relative_path != nullptr &&
         relative_path[0] != '\0' && port_.play(port_.context, relative_path);
}

bool AudioPlayback::pause() noexcept {
  return port_.valid() && port_.pause(port_.context);
}

bool AudioPlayback::resume() noexcept {
  return port_.valid() && port_.resume(port_.context);
}

bool AudioPlayback::stop() noexcept {
  return port_.valid() && port_.stop(port_.context);
}

bool AudioPlayback::set_volume(std::uint8_t percent) noexcept {
  return port_.valid() && percent <= 100U &&
         port_.set_volume(port_.context, percent);
}

AudioProbeResult AudioPlayback::probe(const char* relative_path,
                                      AudioFileInfo& output) const noexcept {
  output = {};
  if (!port_.valid() || relative_path == nullptr || relative_path[0] == '\0') {
    return AudioProbeResult::unavailable;
  }
  return port_.probe(port_.context, relative_path, output);
}

PlaybackSnapshot AudioPlayback::snapshot() const noexcept {
  return port_.valid() ? port_.snapshot(port_.context) : PlaybackSnapshot{};
}

PlaybackProgress AudioPlayback::progress() const noexcept {
  return progress_of(snapshot());
}

PlaybackProgress AudioPlayback::progress_of(PlaybackSnapshot value) noexcept {
  PlaybackProgress result;
  result.total_ms = value.duration_ms;
  result.duration_known = value.duration_ms > 0U;
  result.elapsed_ms = result.duration_known && value.position_ms > value.duration_ms
                          ? value.duration_ms
                          : value.position_ms;
  result.remaining_ms = result.duration_known
                            ? value.duration_ms - result.elapsed_ms
                            : 0U;
  if (result.duration_known) {
    // 긴 파일에서도 32-bit 곱셈 overflow 없이 0~1000 범위를 계산한다.
    const std::uint64_t scaled =
        static_cast<std::uint64_t>(result.elapsed_ms) * 1000U;
    const std::uint64_t permille = scaled / value.duration_ms;
    result.permille = static_cast<std::uint16_t>(permille > 1000U ? 1000U
                                                                  : permille);
  }
  return result;
}

}  // namespace hmi::audio

