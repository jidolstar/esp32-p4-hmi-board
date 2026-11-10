#pragma once

#include <array>

#include "hmi/audio/playback_port.hpp"

namespace hmi::boards::jc1060p470c {
class Board;
}

namespace hmi::platform::audio {

/**
 * profile 상대 asset path를 JC1060P470C public Audio facade에 연결하는 adapter다.
 *
 * Board는 이 객체보다 오래 살아야 하며 adapter는 Board internal이나 SD handle을 노출하지
 * 않는다. 반환한 port도 adapter lifetime 안에서만 사용한다.
 */
class BoardWavPlayback {
 public:
  BoardWavPlayback(hmi::boards::jc1060p470c::Board& board,
                   const char* asset_storage_root) noexcept;

  [[nodiscard]] hmi::audio::AudioPlaybackPort port() noexcept;
  [[nodiscard]] bool valid() const noexcept { return valid_; }

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

  hmi::boards::jc1060p470c::Board& board_;
  std::array<char, 320U> root_{};
  bool valid_ = false;
};

}  // namespace hmi::platform::audio

