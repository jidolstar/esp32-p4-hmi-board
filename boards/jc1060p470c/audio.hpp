#pragma once

#include <cstdint>

namespace hmi::boards::jc1060p470c {
namespace internal {
class AudioService;
}

enum class AudioState : std::uint8_t { idle, starting, playing, error };
enum class AudioError : std::uint8_t {
  none,
  unavailable,
  path,
  file,
  format,
  truncated,
  output,
  read,
};

struct AudioStatus {
  AudioState state = AudioState::idle;
  AudioError error = AudioError::none;
  std::uint8_t volume = 100U;
};

/**
 * Board가 소유한 WAV worker에 재생·정지·음량 요청을 전달하는 비소유 facade다.
 *
 * 실제 SD stream, ES8311, I2S와 task lifetime은 internal::AudioService가 숨긴다.
 * 호출자는 Board보다 이 facade를 오래 보관하면 안 된다.
 */
class Audio {
 public:
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  bool play(const char* path);
  bool stop();
  bool set_volume(std::uint8_t volume);
  [[nodiscard]] AudioStatus status() const noexcept;

 private:
  friend class Board;
  explicit Audio(internal::AudioService& service) : service_(&service) {}

  internal::AudioService* service_;
};

static_assert(sizeof(Audio) == sizeof(void*),
              "Audio facade must remain pointer-sized");

}  // namespace hmi::boards::jc1060p470c
