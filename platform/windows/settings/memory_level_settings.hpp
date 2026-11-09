#pragma once

#include <array>
#include <cstdint>

#include <SDL.h>

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
                      const char* asset_root) noexcept;
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

  bool open_audio(const SDL_AudioSpec& source_spec) noexcept;
  bool play_preview_file() noexcept;
  bool stop_preview_file() noexcept;

  hmi::platform::windows::SdlLvglPort* display_ = nullptr;
  std::array<char, 320U> preview_path_{};
  SDL_AudioDeviceID audio_device_ = 0U;
  SDL_AudioSpec audio_spec_{};
  std::uint8_t stored_brightness_ = 10U;
  std::uint8_t stored_volume_ = 10U;
  std::uint8_t applied_brightness_ = 10U;
  std::uint8_t applied_volume_ = 10U;
  unsigned preview_count_ = 0U;
  bool has_brightness_ = false;
  bool has_volume_ = false;
  bool audio_subsystem_owned_ = false;
  bool preview_path_valid_ = false;
};

}  // namespace hmi::platform::settings
