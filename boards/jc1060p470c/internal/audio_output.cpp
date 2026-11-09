#include "internal/audio_output.hpp"

#include <Arduino.h>
#include <freertos/task.h>

#include "config.hpp"
#include "internal/i2c_transaction.hpp"
#include "internal/wav_reader.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

std::uint8_t volume_register(std::uint8_t percent) {
  if (percent == 0U) {
    return 0U;
  }
  if (percent <= 50U) {
    return static_cast<std::uint8_t>(
        0x47U + (static_cast<unsigned>(percent) - 1U) *
                    (0x82U - 0x47U) / 49U);
  }
  const unsigned bounded = percent > 100U ? 100U : percent;
  return static_cast<std::uint8_t>(
      0x82U + (bounded - 50U) * (0xAFU - 0x82U) / 50U);
}

}  // namespace

AudioOutput::~AudioOutput() { end(); }

bool AudioOutput::codec_write(std::uint8_t reg, std::uint8_t value) {
  I2cTransaction transaction(20U);
  if (!transaction.acquired()) {
    return false;
  }
  wire_.beginTransmission(config::kAudioCodecAddress);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission() == 0U;
}

bool AudioOutput::codec_init(std::uint32_t sample_rate) {
  {
    I2cTransaction transaction(20U);
    if (!transaction.acquired()) {
      return false;
    }
    wire_.beginTransmission(config::kAudioCodecAddress);
    if (wire_.endTransmission() != 0U) {
      return false;
    }
  }
  static constexpr std::uint8_t sequence[][2] = {
      {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24},
      {0x04, 0x10}, {0x05, 0x00}, {0x0b, 0x00}, {0x0c, 0x00},
      {0x10, 0x1f}, {0x11, 0x7f}, {0x00, 0x80}, {0x00, 0x80},
      {0x01, 0x3f}, {0x01, 0x3f}, {0x02, 0x00}, {0x05, 0x00},
      {0x03, 0x10}, {0x04, 0x10}, {0x07, 0x00}, {0x08, 0xff},
      {0x06, 0x03}, {0x13, 0x10}, {0x1b, 0x0a}, {0x1c, 0x6a},
      {0x09, 0x0c}, {0x0a, 0x4c}, {0x17, 0xbf}, {0x0e, 0x02},
      {0x12, 0x00}, {0x14, 0x1a}, {0x0d, 0x01}, {0x15, 0x40},
      {0x37, 0x48}, {0x45, 0x00}, {0x31, 0x60}, {0x32, 0x00},
  };
  for (const auto& entry : sequence) {
    if (!codec_write(entry[0], entry[1])) {
      return false;
    }
  }
  // MCLK=256fs에서 8/16 kHz는 다른 DAC oversampling 설정을 사용한다.
  return sample_rate > 16000U || codec_write(0x04, 0x20);
}

bool AudioOutput::begin(std::uint32_t sample_rate) {
  end();
  if (!wav_sample_rate_supported(sample_rate)) {
    return false;
  }
  pinMode(config::kAudioPa, OUTPUT);
  digitalWrite(config::kAudioPa, LOW);

  const i2s_chan_config_t channel =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&channel, &tx_, nullptr) != ESP_OK) {
    return false;
  }
  i2s_std_config_t standard{};
  standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
  standard.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  standard.gpio_cfg.mclk = static_cast<gpio_num_t>(config::kAudioMclk);
  standard.gpio_cfg.bclk = static_cast<gpio_num_t>(config::kAudioBclk);
  standard.gpio_cfg.ws = static_cast<gpio_num_t>(config::kAudioWs);
  standard.gpio_cfg.dout = static_cast<gpio_num_t>(config::kAudioDout);
  standard.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(tx_, &standard) != ESP_OK ||
      i2s_channel_enable(tx_) != ESP_OK || !codec_init(sample_rate)) {
    end();
    return false;
  }
  sample_rate_ = sample_rate;
  return true;
}

bool AudioOutput::set_volume(std::uint8_t percent) {
  return tx_ != nullptr && percent <= 100U &&
         codec_write(0x32, volume_register(percent));
}

bool AudioOutput::set_muted(bool muted) {
  if (tx_ == nullptr || !codec_write(0x31, muted ? 0x60U : 0x00U)) {
    return false;
  }
  set_pa(!muted);
  return true;
}

void AudioOutput::set_pa(bool enabled) {
  digitalWrite(config::kAudioPa, enabled ? HIGH : LOW);
}

bool AudioOutput::write(const std::int16_t* samples, std::size_t count) {
  if (tx_ == nullptr || samples == nullptr || sample_rate_ == 0U) {
    return false;
  }
  std::size_t written = 0U;
  const std::uint32_t frames = static_cast<std::uint32_t>(count / 2U);
  const std::uint32_t timeout_ms =
      20U + (frames * 1000U + sample_rate_ - 1U) / sample_rate_;
  return i2s_channel_write(tx_, samples, count * sizeof(*samples), &written,
                           timeout_ms) == ESP_OK &&
         written == count * sizeof(*samples);
}

void AudioOutput::end() noexcept {
  pinMode(config::kAudioPa, OUTPUT);
  if (tx_ != nullptr) {
    codec_write(0x32, 0x00);
    codec_write(0x31, 0x60);
    vTaskDelay(pdMS_TO_TICKS(10U));
  }
  digitalWrite(config::kAudioPa, LOW);
  if (tx_ != nullptr) {
    i2s_channel_disable(tx_);
    i2s_del_channel(tx_);
    tx_ = nullptr;
  }
  sample_rate_ = 0U;
}

}  // namespace hmi::boards::jc1060p470c::internal
