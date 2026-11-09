#include "hmi/settings/level_settings.hpp"

namespace hmi::settings {

bool LevelSettings::begin(LevelSettingsPort port,
                          std::uint8_t initial_brightness,
                          std::uint8_t initial_volume) noexcept {
  port_ = port;
  ready_ = false;
  volume_pending_ = false;
  if (port_.load == nullptr || port_.save_brightness == nullptr ||
      port_.save_volume == nullptr || port_.apply_brightness == nullptr ||
      port_.apply_volume == nullptr || initial_brightness < 1U ||
      initial_brightness > 10U || initial_volume > 10U) {
    return false;
  }

  std::uint8_t brightness = initial_brightness;
  std::uint8_t volume = initial_volume;
  bool has_brightness = false;
  bool has_volume = false;
  if (!port_.load(port_.context, brightness, volume, has_brightness,
                  has_volume)) {
    return false;
  }
  if (!has_brightness) {
    brightness = initial_brightness;
    if (!port_.save_brightness(port_.context, brightness)) {
      return false;
    }
  }
  if (!has_volume) {
    volume = initial_volume;
    if (!port_.save_volume(port_.context, volume)) {
      return false;
    }
  }

  brightness_ = brightness >= 1U && brightness <= 10U ? brightness : 10U;
  volume_ = volume <= 10U ? volume : 10U;
  // 저장값을 먼저 실제 출력에 복원해 page가 model과 다른 값을 표시하지 않게 한다.
  if (!port_.apply_brightness(port_.context, brightness_)) {
    return false;
  }
  volume_pending_ = !port_.apply_volume(port_.context, volume_);
  ready_ = true;
  return true;
}

void LevelSettings::end() noexcept {
  if (ready_ && port_.stop_preview != nullptr) {
    port_.stop_preview(port_.context);
  }
  ready_ = false;
  volume_pending_ = false;
}

LevelSettingsResult LevelSettings::set_brightness(std::uint8_t level) noexcept {
  if (level < 1U || level > 10U) {
    return LevelSettingsResult::invalid;
  }
  if (!ready_) {
    return LevelSettingsResult::unavailable;
  }
  if (level == brightness_) {
    return LevelSettingsResult::unchanged;
  }
  if (!port_.apply_brightness(port_.context, level)) {
    return LevelSettingsResult::apply_failed;
  }
  if (!port_.save_brightness(port_.context, level)) {
    port_.apply_brightness(port_.context, brightness_);
    return LevelSettingsResult::save_failed;
  }
  brightness_ = level;
  return LevelSettingsResult::ok;
}

LevelSettingsResult LevelSettings::set_volume(std::uint8_t level) noexcept {
  if (level > 10U) {
    return LevelSettingsResult::invalid;
  }
  if (!ready_) {
    return LevelSettingsResult::unavailable;
  }
  if (level == volume_) {
    return LevelSettingsResult::unchanged;
  }
  if (!port_.save_volume(port_.context, level)) {
    return LevelSettingsResult::save_failed;
  }
  volume_ = level;
  volume_pending_ = !port_.apply_volume(port_.context, level);
  if (volume_pending_) {
    return LevelSettingsResult::apply_failed;
  }
  if (level == 0U) {
    return port_.stop_preview != nullptr &&
                   !port_.stop_preview(port_.context)
               ? LevelSettingsResult::preview_unavailable
               : LevelSettingsResult::ok;
  }
  return port_.play_preview != nullptr &&
                 !port_.play_preview(port_.context)
             ? LevelSettingsResult::preview_unavailable
             : LevelSettingsResult::ok;
}

LevelSettingsResult LevelSettings::play_preview() noexcept {
  if (!ready_) {
    return LevelSettingsResult::unavailable;
  }
  if (volume_ == 0U) {
    return LevelSettingsResult::unchanged;
  }
  retry_volume();
  if (volume_pending_) {
    return LevelSettingsResult::apply_failed;
  }
  return port_.play_preview != nullptr &&
                 !port_.play_preview(port_.context)
             ? LevelSettingsResult::preview_unavailable
             : LevelSettingsResult::ok;
}

void LevelSettings::retry_volume() noexcept {
  if (ready_ && volume_pending_ &&
      port_.apply_volume(port_.context, volume_)) {
    volume_pending_ = false;
  }
}

}  // namespace hmi::settings
