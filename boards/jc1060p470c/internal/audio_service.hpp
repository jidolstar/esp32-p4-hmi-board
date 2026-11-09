#pragma once

#include <atomic>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "audio.hpp"

class TwoWire;

namespace hmi::boards::jc1060p470c::internal {
class SdStorage;

/**
 * Board가 값으로 소유하는 단일 WAV worker다.
 *
 * 명령 queue는 마지막 요청 하나만 유지한다. WAV 전체를 적재하지 않고 worker stack의
 * 고정 chunk만 사용하며 SD와 codec 세부사항을 Audio facade 밖으로 노출하지 않는다.
 */
class AudioService {
 public:
  AudioService(SdStorage& storage, TwoWire& wire)
      : storage_(storage), wire_(wire) {}
  AudioService(const AudioService&) = delete;
  AudioService& operator=(const AudioService&) = delete;

  bool begin();
  bool play(const char* path);
  bool stop();
  bool set_volume(std::uint8_t volume);
  [[nodiscard]] AudioStatus status() const noexcept;

 private:
  enum class CommandType : std::uint8_t { play, stop };
  struct Command {
    CommandType type = CommandType::stop;
    char path[257]{};
  };

  static void worker_entry(void* context);
  void worker();
  AudioError play_file(const char* path, Command& next, bool& has_next);
  void set(AudioState state, AudioError error) noexcept;

  SdStorage& storage_;
  TwoWire& wire_;
  QueueHandle_t queue_ = nullptr;
  std::atomic<AudioState> state_{AudioState::idle};
  std::atomic<AudioError> error_{AudioError::none};
  std::atomic<std::uint8_t> volume_{100U};
};

static_assert(sizeof(AudioService) <= 64U,
              "AudioService state must stay bounded; PCM belongs to worker stack");

}  // namespace hmi::boards::jc1060p470c::internal
