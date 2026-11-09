#pragma once

#include <cstdint>

namespace hmi::boards::jc1060p470c::config {

inline constexpr const char *kName = "JC1060P470C-I-W-Y";
inline constexpr std::uint16_t kScreenWidth = 1024;
inline constexpr std::uint16_t kScreenHeight = 600;
inline constexpr std::uint32_t kSerialBaud = 115200;

inline constexpr std::uint8_t kTouchSda = 7;
inline constexpr std::uint8_t kTouchScl = 8;
inline constexpr std::uint8_t kTouchIrq = 21;
inline constexpr std::uint8_t kTouchReset = 22;
inline constexpr std::uint32_t kTouchI2cFrequency = 100000;

// 제조사 ES8311 예제 배선. touch와 codec은 Board의 같은 I2C bus를 공유한다.
inline constexpr std::uint8_t kAudioCodecAddress = 0x18;
inline constexpr std::uint8_t kAudioMclk = 13;
inline constexpr std::uint8_t kAudioBclk = 12;
inline constexpr std::uint8_t kAudioWs = 10;
inline constexpr std::uint8_t kAudioDout = 9;
inline constexpr std::uint8_t kAudioPa = 11;

// JC1060P470C TF 슬롯은 ESP32-P4 SDMMC 4-bit IOMUX를 사용한다.
// LDO4와 SD 전원 GPIO 설정은 선택된 Arduino esp32p4 보드 variant를 따른다.
inline constexpr std::uint8_t kSdClock = 43;
inline constexpr std::uint8_t kSdCommand = 44;
inline constexpr std::uint8_t kSdData0 = 39;
inline constexpr std::uint8_t kSdData1 = 40;
inline constexpr std::uint8_t kSdData2 = 41;
inline constexpr std::uint8_t kSdData3 = 42;
inline constexpr char kSdMountPath[] = "/sdcard";

}  // namespace hmi::boards::jc1060p470c::config
