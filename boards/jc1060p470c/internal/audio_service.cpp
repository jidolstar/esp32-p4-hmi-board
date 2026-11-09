#include "internal/audio_service.hpp"

#include <Arduino.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include <freertos/task.h>

#include "config.hpp"
#include "internal/audio_output.hpp"
#include "internal/sd_storage.hpp"
#include "internal/wav_reader.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

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
  // local PCM buffer 약 1.5 KiB와 codec 호출 여유를 포함하되 큰 객체는 만들지 않는다.
  if (xTaskCreate(worker_entry, "wav_player", 6144U, this, 2U, nullptr) !=
      pdPASS) {
    vQueueDelete(queue_);
    queue_ = nullptr;
    return false;
  }
  return true;
}

bool AudioService::play(const char* path) {
  if (queue_ == nullptr || !valid_path(path) || !storage_.exists(path)) {
    return false;
  }
  Command command{};
  command.type = CommandType::play;
  std::strcpy(command.path, path);
  return xQueueOverwrite(queue_, &command) == pdTRUE;
}

bool AudioService::stop() {
  if (queue_ == nullptr) {
    return false;
  }
  Command command{};
  command.type = CommandType::stop;
  return xQueueOverwrite(queue_, &command) == pdTRUE;
}

bool AudioService::set_volume(std::uint8_t volume) {
  if (queue_ == nullptr || volume > 100U) {
    return false;
  }
  volume_.store(volume);
  return true;
}

AudioStatus AudioService::status() const noexcept {
  return {state_.load(), error_.load(), volume_.load()};
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
      set(AudioState::idle, AudioError::none);
      continue;
    }
    const AudioError result = play_file(command.path, command, pending);
    set(result == AudioError::none ? AudioState::idle : AudioState::error,
        result);
  }
}

AudioError AudioService::play_file(const char* path, Command& next,
                                   bool& has_next) {
  set(AudioState::starting, AudioError::none);
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
      break;
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
    if (applied_volume != 0U && !output.write(slots, samples)) {
      result = AudioError::output;
      break;
    }
    remaining -= static_cast<std::uint32_t>(count);
    offset += count;
  }
  output.end();
  std::fclose(file);
  return result;
}

}  // namespace hmi::boards::jc1060p470c::internal
