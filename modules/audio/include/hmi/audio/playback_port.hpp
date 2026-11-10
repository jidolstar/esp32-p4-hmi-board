#pragma once

#include <cstdint>

namespace hmi::audio {

enum class PlaybackState : std::uint8_t {
  unavailable,
  idle,
  starting,
  playing,
  pausing,
  paused,
  resuming,
  completed,
  stopped,
  error,
};

enum class PlaybackError : std::uint8_t {
  none,
  unavailable,
  path,
  file,
  unsupported,
  truncated,
  output,
  read,
  busy,
};

enum class AudioProbeResult : std::uint8_t {
  ok,
  unavailable,
  file,
  invalid,
  truncated,
  unsupported,
};

struct AudioFileInfo {
  std::uint64_t file_bytes = 0U;
  std::uint32_t sample_rate_hz = 0U;
  std::uint32_t duration_ms = 0U;
  std::uint16_t channels = 0U;
  std::uint16_t bits_per_sample = 0U;
};

struct PlaybackSnapshot {
  PlaybackState state = PlaybackState::unavailable;
  PlaybackError error = PlaybackError::unavailable;
  std::uint32_t position_ms = 0U;
  std::uint32_t duration_ms = 0U;
  std::uint32_t revision = 0U;
  std::uint8_t volume_percent = 100U;
};

/**
 * 공통 playback model이 사용하는 비소유 platform 함수 표다.
 *
 * context와 함수 포인터의 실제 객체는 port를 사용하는 model보다 오래 살아야 한다.
 * path는 profile asset root 기준 상대 경로이며 platform handle과 worker 세부사항은
 * 이 경계를 통과하지 않는다.
 */
struct AudioPlaybackPort {
  using Play = bool (*)(void* context, const char* relative_path) noexcept;
  using Pause = bool (*)(void* context) noexcept;
  using Resume = bool (*)(void* context) noexcept;
  using Stop = bool (*)(void* context) noexcept;
  using SetVolume = bool (*)(void* context, std::uint8_t percent) noexcept;
  using Probe = AudioProbeResult (*)(void* context, const char* relative_path,
                                     AudioFileInfo& output) noexcept;
  using Snapshot = PlaybackSnapshot (*)(void* context) noexcept;

  void* context = nullptr;
  Play play = nullptr;
  Stop stop = nullptr;
  SetVolume set_volume = nullptr;
  Probe probe = nullptr;
  Snapshot snapshot = nullptr;
  Pause pause = nullptr;
  Resume resume = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return context != nullptr && play != nullptr && stop != nullptr &&
           set_volume != nullptr && probe != nullptr && snapshot != nullptr &&
           pause != nullptr && resume != nullptr;
  }
};

}  // namespace hmi::audio

