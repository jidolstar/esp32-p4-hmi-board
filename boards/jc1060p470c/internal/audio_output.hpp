#pragma once

#include <cstddef>
#include <cstdint>

#include <Wire.h>
#include <driver/i2s_std.h>

namespace hmi::boards::jc1060p470c::internal {

/** ES8311 mono DAC와 ESP32-P4 I2S TX의 수명을 한 재생 session에 묶는다. */
class AudioOutput {
 public:
  explicit AudioOutput(TwoWire& wire) : wire_(wire) {}
  AudioOutput(const AudioOutput&) = delete;
  AudioOutput& operator=(const AudioOutput&) = delete;
  ~AudioOutput();

  bool begin(std::uint32_t sample_rate);
  bool set_volume(std::uint8_t percent);
  bool set_muted(bool muted);
  bool write(const std::int16_t* samples, std::size_t count);
  void end() noexcept;

 private:
  bool codec_write(std::uint8_t reg, std::uint8_t value);
  bool codec_init(std::uint32_t sample_rate);
  void set_pa(bool enabled);

  TwoWire& wire_;
  i2s_chan_handle_t tx_ = nullptr;
  std::uint32_t sample_rate_ = 0U;
};

}  // namespace hmi::boards::jc1060p470c::internal
