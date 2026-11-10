#pragma once

#include <cstdint>

#include "hmi/audio/playback_port.hpp"

namespace hmi::audio {

struct PlaybackProgress {
  std::uint32_t elapsed_ms = 0U;
  std::uint32_t total_ms = 0U;
  std::uint32_t remaining_ms = 0U;
  std::uint16_t permille = 0U;
  bool duration_known = false;
};

/**
 * platform port를 검증하고 시간·progress 계산을 공통화하는 작은 controller다.
 *
 * port는 비소유 view이며 AudioPlayback보다 오래 살아야 한다. controller는 파일이나
 * PCM buffer를 소유하지 않고 재생·일시정지·이어 재생·정지 명령만 전달한다.
 * end에서는 stop을 요청한 뒤 port 연결을 해제한다.
 */
class AudioPlayback {
 public:
  AudioPlayback() = default;
  ~AudioPlayback() { end(); }
  AudioPlayback(const AudioPlayback&) = delete;
  AudioPlayback& operator=(const AudioPlayback&) = delete;

  bool begin(AudioPlaybackPort port) noexcept;
  void end() noexcept;
  bool play(const char* relative_path) noexcept;
  bool pause() noexcept;
  bool resume() noexcept;
  bool stop() noexcept;
  bool set_volume(std::uint8_t percent) noexcept;
  AudioProbeResult probe(const char* relative_path,
                         AudioFileInfo& output) const noexcept;
  [[nodiscard]] PlaybackSnapshot snapshot() const noexcept;
  [[nodiscard]] PlaybackProgress progress() const noexcept;
  [[nodiscard]] bool ready() const noexcept { return port_.valid(); }

  static PlaybackProgress progress_of(PlaybackSnapshot value) noexcept;

 private:
  AudioPlaybackPort port_{};
};

static_assert(sizeof(AudioPlayback) <= 64U,
              "AudioPlayback must remain a small non-owning controller");

}  // namespace hmi::audio

