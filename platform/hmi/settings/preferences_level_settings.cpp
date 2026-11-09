#include "settings/preferences_level_settings.hpp"

#include <Preferences.h>

#include <cstdio>
#include <cstring>

#include "board.hpp"

namespace hmi::platform::settings {
namespace {
constexpr char kBrightnessKey[] = "disp_level";
constexpr char kVolumeKey[] = "sound_level";
constexpr char kPreviewRelativePath[] = "/assets/sounds/volume_preview.wav";
}

PreferencesLevelSettings::PreferencesLevelSettings(
    hmi::boards::jc1060p470c::Board& board, const char* nvs_namespace,
    const char* asset_storage_root) noexcept
    : board_(board), nvs_namespace_(nvs_namespace) {
  if (nvs_namespace == nullptr || nvs_namespace[0] == '\0' ||
      asset_storage_root == nullptr || asset_storage_root[0] != '/') {
    return;
  }
  const int written = std::snprintf(
      preview_path_.data(), preview_path_.size(), "%s%s", asset_storage_root,
      kPreviewRelativePath);
  valid_ = written > 0 &&
           static_cast<std::size_t>(written) < preview_path_.size();
}

hmi::settings::LevelSettingsPort PreferencesLevelSettings::port() noexcept {
  return valid_ ? hmi::settings::LevelSettingsPort{
                      this, load, save_brightness, save_volume,
                      apply_brightness, apply_volume, play_preview, stop_preview}
                : hmi::settings::LevelSettingsPort{};
}

bool PreferencesLevelSettings::load(void* context, std::uint8_t& brightness,
                                    std::uint8_t& volume,
                                    bool& has_brightness,
                                    bool& has_volume) {
  auto& self = *static_cast<PreferencesLevelSettings*>(context);
  Preferences preferences;
  if (!self.valid_ || !preferences.begin(self.nvs_namespace_, false)) {
    return false;
  }
  has_brightness = preferences.isKey(kBrightnessKey);
  has_volume = preferences.isKey(kVolumeKey);
  brightness = preferences.getUChar(kBrightnessKey, 10U);
  volume = preferences.getUChar(kVolumeKey, 10U);
  preferences.end();
  return true;
}

bool PreferencesLevelSettings::save(const char* key,
                                    std::uint8_t value) const {
  Preferences preferences;
  if (!valid_ || key == nullptr ||
      !preferences.begin(nvs_namespace_, false)) {
    return false;
  }
  const bool saved = preferences.putUChar(key, value) == 1U;
  preferences.end();
  return saved;
}

bool PreferencesLevelSettings::save_brightness(void* context,
                                                std::uint8_t value) {
  return static_cast<PreferencesLevelSettings*>(context)->save(kBrightnessKey,
                                                               value);
}

bool PreferencesLevelSettings::save_volume(void* context,
                                            std::uint8_t value) {
  return static_cast<PreferencesLevelSettings*>(context)->save(kVolumeKey,
                                                               value);
}

bool PreferencesLevelSettings::apply_brightness(void* context,
                                                 std::uint8_t value) {
  return static_cast<PreferencesLevelSettings*>(context)
      ->board_.set_backlight_level(value);
}

bool PreferencesLevelSettings::apply_volume(void* context,
                                             std::uint8_t value) {
  return static_cast<PreferencesLevelSettings*>(context)
      ->board_.audio().set_volume(static_cast<std::uint8_t>(value * 10U));
}

bool PreferencesLevelSettings::play_preview(void* context) {
  auto& self = *static_cast<PreferencesLevelSettings*>(context);
  return self.board_.audio().play(self.preview_path_.data());
}

bool PreferencesLevelSettings::stop_preview(void* context) {
  return static_cast<PreferencesLevelSettings*>(context)->board_.audio().stop();
}

}  // namespace hmi::platform::settings
