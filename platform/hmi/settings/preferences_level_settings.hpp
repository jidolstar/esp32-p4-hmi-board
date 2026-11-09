#pragma once

#include <array>

#include "hmi/settings/level_settings_port.hpp"

namespace hmi::boards::jc1060p470c {
class Board;
}

namespace hmi::platform::settings {

/**
 * sample profile의 NVS 값과 JC1060P470C public facade를 LevelSettingsPort로 묶는다.
 *
 * Board와 문자열 인자는 이 객체보다 오래 살아야 한다. preview 경로는 생성 시 고정 크기
 * 배열로 복사하며 Preferences와 hardware 세부사항을 공통 model에 노출하지 않는다.
 */
class PreferencesLevelSettings {
 public:
  PreferencesLevelSettings(hmi::boards::jc1060p470c::Board& board,
                           const char* nvs_namespace,
                           const char* asset_storage_root) noexcept;

  [[nodiscard]] hmi::settings::LevelSettingsPort port() noexcept;
  [[nodiscard]] bool valid() const noexcept { return valid_; }

 private:
  static bool load(void*, std::uint8_t&, std::uint8_t&, bool&, bool&);
  static bool save_brightness(void*, std::uint8_t);
  static bool save_volume(void*, std::uint8_t);
  static bool apply_brightness(void*, std::uint8_t);
  static bool apply_volume(void*, std::uint8_t);
  static bool play_preview(void*);
  static bool stop_preview(void*);

  bool save(const char* key, std::uint8_t value) const;

  hmi::boards::jc1060p470c::Board& board_;
  const char* nvs_namespace_ = nullptr;
  std::array<char, 320U> preview_path_{};
  bool valid_ = false;
};

}  // namespace hmi::platform::settings
