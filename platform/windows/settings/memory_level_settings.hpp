#pragma once

#include <cstdint>

#include "hmi/audio/playback_port.hpp"
#include "hmi/settings/level_settings_port.hpp"

namespace hmi::platform::windows {
class SdlLvglPort;
}

namespace hmi::platform::settings {

/**
 * Windows sample에서 설정 저장과 simulator 밝기·WAV 출력을 연결하는 adapter다.
 *
 * OS 전체 밝기·음량은 변경하지 않는다. display와 asset root를 생략하면 단위 test용
 * memory adapter로 동작한다. host가 연결한 경우 실제 simulator 출력 실패를 숨기지 않는다.
 */
class MemoryLevelSettings {
 public:
  MemoryLevelSettings() = default;
  MemoryLevelSettings(hmi::platform::windows::SdlLvglPort& display,
                      hmi::audio::AudioPlaybackPort playback) noexcept;
  ~MemoryLevelSettings();
  MemoryLevelSettings(const MemoryLevelSettings&) = delete;
  MemoryLevelSettings& operator=(const MemoryLevelSettings&) = delete;

  [[nodiscard]] hmi::settings::LevelSettingsPort port() noexcept;
  [[nodiscard]] std::uint8_t applied_brightness() const noexcept;
  [[nodiscard]] std::uint8_t applied_volume() const noexcept;
  [[nodiscard]] unsigned preview_count() const noexcept;

 private:
  static bool load(void*, std::uint8_t&, std::uint8_t&, bool&, bool&);
  static bool save_brightness(void*, std::uint8_t);
  static bool save_volume(void*, std::uint8_t);
  static bool apply_brightness(void*, std::uint8_t);
  static bool apply_volume(void*, std::uint8_t);
  static bool play_preview(void*);
  static bool stop_preview(void*);

  bool play_preview_file() noexcept;
  bool stop_preview_file() noexcept;

  hmi::platform::windows::SdlLvglPort* display_ = nullptr;
  hmi::audio::AudioPlaybackPort playback_{};
  std::uint8_t stored_brightness_ = 10U;
  std::uint8_t stored_volume_ = 10U;
  std::uint8_t applied_brightness_ = 10U;
  std::uint8_t applied_volume_ = 10U;
  unsigned preview_count_ = 0U;
  bool has_brightness_ = false;
  bool has_volume_ = false;
};

}  // namespace hmi::platform::settings
