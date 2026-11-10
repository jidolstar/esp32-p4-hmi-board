#include "internal/audio_service.hpp"

#include <Arduino.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include <freertos/task.h>

#include "config.hpp"
#include "internal/audio_output.hpp"
#include "internal/wav_reader.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

// I2S/stdio/codec 호출과 1.5 KiB PCM chunk가 동시에 살아도 worker stack을 넘지 않게 한다.
// 202609 실기기 재생 경로에서 검증된 예산이며 UI task stack과는 분리된다.
constexpr std::uint32_t kWorkerStackBytes = 8192U;

bool valid_path(const char* path) {
  return path != nullptr && path[0] == '/' && std::strlen(path) <= 256U &&
         std::strstr(path, "..") == nullptr;
}

bool read_at(void* context, std::uint64_t offset, std::uint8_t* output,
             std::size_t size) {
  auto* file = static_cast<std::FILE*>(context);
  return file != nullptr && output != nullptr && offset <= LONG_MAX &&
         std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
         std::fread(output, 1U, size, file) == size;
}

}  // namespace

bool AudioService::begin() {
  pinMode(config::kAudioPa, OUTPUT);
  digitalWrite(config::kAudioPa, LOW);
  if (queue_ != nullptr) {
    return true;
  }
  queue_ = xQueueCreate(1U, sizeof(Command));
  if (queue_ == nullptr) {
    return false;
  }
  if (xTaskCreate(worker_entry, "wav_player", kWorkerStackBytes, this, 2U,
                  nullptr) !=
      pdPASS) {
    vQueueDelete(queue_);
    queue_ = nullptr;
    return false;
  }
  return true;
}

bool AudioService::play(const char* path) {
  if (queue_ == nullptr || !valid_path(path)) {
    return false;
  }
  Command command{};
  command.type = CommandType::play;
  std::strcpy(command.path, path);
  if (xQueueOverwrite(queue_, &command) != pdTRUE) {
    return false;
  }
  // touch/LVGL callback에서는 SD stat/open을 하지 않는다. 파일 검증과 오류 전환은
  // worker가 lease를 얻은 뒤 처리해 클릭 응답 시간을 저장장치 지연과 분리한다.
  set(AudioState::starting, AudioError::none);
  revision_.fetch_add(1U);
  return true;
}

bool AudioService::pause() {
  AudioState expected = AudioState::playing;
  if (queue_ == nullptr ||
      !state_.compare_exchange_strong(expected, AudioState::pausing)) {
    return false;
  }
  Command command{};
  command.type = CommandType::pause;
  if (xQueueOverwrite(queue_, &command) != pdTRUE) {
    state_.store(AudioState::playing);
    return false;
  }
  revision_.fetch_add(1U);
  return true;
}

bool AudioService::resume() {
  AudioState expected = AudioState::paused;
  if (queue_ == nullptr ||
      !state_.compare_exchange_strong(expected, AudioState::resuming)) {
    return false;
  }
  Command command{};
  command.type = CommandType::resume;
  if (xQueueOverwrite(queue_, &command) != pdTRUE) {
    state_.store(AudioState::paused);
    return false;
  }
  revision_.fetch_add(1U);
  return true;
}

bool AudioService::stop() {
  if (queue_ == nullptr) {
    return false;
  }
  Command command{};
  command.type = CommandType::stop;
  if (xQueueOverwrite(queue_, &command) != pdTRUE) {
    return false;
  }
  revision_.fetch_add(1U);
  return true;
}

bool AudioService::set_volume(std::uint8_t volume) {
  if (queue_ == nullptr || volume > 100U) {
    return false;
  }
  volume_.store(volume);
  return true;
}

AudioStatus AudioService::status() const noexcept {
  return {state_.load(),       error_.load(),    position_ms_.load(),
          duration_ms_.load(), revision_.load(), volume_.load()};
}

AudioProbeResult AudioService::probe(const char* path,
                                     AudioFileInfo& output) const {
  output = {};
  if (!valid_path(path)) {
    return AudioProbeResult::invalid;
  }
  auto storage_lease = storage_.acquire_exclusive_lease();
  if (!storage_lease) {
    return AudioProbeResult::unavailable;
  }
  std::FILE* file = storage_.open_read_stream(path);
  if (file == nullptr) {
    return storage_.ready() ? AudioProbeResult::file
                            : AudioProbeResult::unavailable;
  }
  struct stat metadata {};
  WavInfo info{};
  WavResult parsed = WavResult::truncated;
  if (::fstat(::fileno(file), &metadata) == 0 && metadata.st_size > 0 &&
      metadata.st_size <= LONG_MAX) {
    parsed = parse_wav(read_at, file,
                       static_cast<std::uint64_t>(metadata.st_size), info);
  }
  std::fclose(file);
  if (parsed != WavResult::ok) {
    if (parsed == WavResult::truncated) {
      return AudioProbeResult::truncated;
    }
    return parsed == WavResult::unsupported ? AudioProbeResult::unsupported
                                            : AudioProbeResult::invalid;
  }
  output.file_bytes = static_cast<std::uint64_t>(metadata.st_size);
  output.sample_rate = info.sample_rate;
  output.duration_ms = wav_milliseconds_for_bytes(info.data_bytes, info);
  output.channels = info.channels;
  output.bits_per_sample = info.bits_per_sample;
  return AudioProbeResult::ok;
}

void AudioService::set(AudioState state, AudioError error) noexcept {
  error_.store(error);
  state_.store(state);
}

void AudioService::worker_entry(void* context) {
  static_cast<AudioService*>(context)->worker();
}

void AudioService::worker() {
  Command command{};
  bool pending = false;
  for (;;) {
    if (pending) {
      xQueueReceive(queue_, &command, 0U);
    } else if (xQueueReceive(queue_, &command, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    pending = false;
    if (command.type == CommandType::stop) {
      position_ms_.store(0U);
      duration_ms_.store(0U);
      set(AudioState::stopped, AudioError::none);
      continue;
    }
    // 활성 stream 밖에 남은 pause/resume 명령은 새 파일로 해석하지 않는다.
    if (command.type != CommandType::play) {
      continue;
    }
    bool stopped = false;
    const AudioError result =
        play_file(command.path, command, pending, stopped);
    if (result != AudioError::none) {
      set(AudioState::error, result);
    } else if (pending) {
      set(AudioState::starting, AudioError::none);
    } else if (stopped) {
      position_ms_.store(0U);
      set(AudioState::stopped, AudioError::none);
    } else {
      position_ms_.store(duration_ms_.load());
      set(AudioState::completed, AudioError::none);
    }
  }
}

AudioError AudioService::play_file(const char* path, Command& next,
                                   bool& has_next, bool& stopped) {
  set(AudioState::starting, AudioError::none);
  position_ms_.store(0U);
  duration_ms_.store(0U);
  stopped = false;
  // 열린 FILE*의 전체 수명 동안 lease를 유지해 USB raw 전환과 겹치지 않게 한다.
  auto storage_lease = storage_.acquire_exclusive_lease();
  if (!storage_lease) {
    return AudioError::unavailable;
  }
  std::FILE* file = storage_.open_read_stream(path);
  if (file == nullptr) {
    return storage_.ready() ? AudioError::file : AudioError::unavailable;
  }
  struct stat metadata {};
  WavInfo info{};
  WavResult parsed = WavResult::truncated;
  if (::fstat(::fileno(file), &metadata) == 0 && metadata.st_size > 0 &&
      metadata.st_size <= LONG_MAX) {
    parsed = parse_wav(read_at, file,
                       static_cast<std::uint64_t>(metadata.st_size), info);
  }
  if (parsed != WavResult::ok) {
    std::fclose(file);
    return parsed == WavResult::truncated ? AudioError::truncated
                                          : AudioError::format;
  }
  duration_ms_.store(wav_milliseconds_for_bytes(info.data_bytes, info));

  AudioOutput output(wire_);
  if (!output.begin(info.sample_rate)) {
    std::fclose(file);
    return AudioError::output;
  }
  std::uint8_t applied_volume = volume_.load();
  if (!output.set_volume(applied_volume) ||
      !output.set_muted(applied_volume == 0U)) {
    std::fclose(file);
    return AudioError::output;
  }
  set(AudioState::playing, AudioError::none);

  // 512-byte input과 256 stereo frame output만 worker stack에 유지한다.
  std::uint8_t input[512]{};
  std::int16_t slots[512]{};
  std::uint32_t remaining = info.data_bytes;
  std::uint64_t offset = info.data_offset;
  const std::size_t frame_bytes = wav_frame_bytes(info);
  const std::size_t maximum_input =
      frame_bytes * (sizeof(slots) / sizeof(slots[0]) / 2U) < sizeof(input)
          ? frame_bytes * (sizeof(slots) / sizeof(slots[0]) / 2U)
          : sizeof(input);
  AudioError result = AudioError::none;
  while (remaining > 0U) {
    if (xQueueReceive(queue_, &next, 0U) == pdTRUE) {
      has_next = next.type == CommandType::play;
      stopped = next.type == CommandType::stop;
      if (has_next || stopped) {
        break;
      }
      if (next.type == CommandType::pause) {
        if (!output.set_muted(true)) {
          result = AudioError::output;
          break;
        }
        // pause 중에도 FILE*, 저장소 lease, offset을 유지한다. resume은 같은
        // chunk 경계에서 이어지고 stop/new play만 이 수명을 끝낸다.
        set(AudioState::paused, AudioError::none);
        for (;;) {
          if (xQueueReceive(queue_, &next, portMAX_DELAY) != pdTRUE) {
            continue;
          }
          if (next.type == CommandType::pause) {
            continue;
          }
          has_next = next.type == CommandType::play;
          stopped = next.type == CommandType::stop;
          if (has_next || stopped) {
            break;
          }
          if (next.type == CommandType::resume) {
            const std::uint8_t requested_volume = volume_.load();
            if (!output.set_volume(requested_volume) ||
                !output.set_muted(requested_volume == 0U)) {
              result = AudioError::output;
            } else {
              applied_volume = requested_volume;
              set(AudioState::playing, AudioError::none);
            }
            break;
          }
        }
        if (has_next || stopped || result != AudioError::none) {
          break;
        }
      }
    }
    const std::uint8_t requested_volume = volume_.load();
    if (requested_volume != applied_volume) {
      if (!output.set_volume(requested_volume) ||
          !output.set_muted(requested_volume == 0U)) {
        result = AudioError::output;
        break;
      }
      applied_volume = requested_volume;
    }
    std::size_t count = remaining < maximum_input ? remaining : maximum_input;
    count -= count % frame_bytes;
    if (count == 0U || !read_at(file, offset, input, count)) {
      result = AudioError::read;
      break;
    }
    const std::size_t samples = convert_pcm_to_stereo_slots(
        input, count, info, slots, sizeof(slots) / sizeof(slots[0]));
    if (samples == 0U) {
      result = AudioError::format;
      break;
    }
    // codec mute 중에도 I2S를 같은 속도로 흘려 progress와 실제 시간이 어긋나지 않게 한다.
    if (!output.write(slots, samples)) {
      result = AudioError::output;
      break;
    }
    remaining -= static_cast<std::uint32_t>(count);
    offset += count;
    position_ms_.store(wav_milliseconds_for_bytes(
        info.data_bytes - remaining, info));
  }
  output.end();
  std::fclose(file);
  return result;
}

}  // namespace hmi::boards::jc1060p470c::internal
