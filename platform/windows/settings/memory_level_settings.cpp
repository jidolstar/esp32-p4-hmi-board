#include "settings/memory_level_settings.hpp"

#include "sdl_lvgl_port.hpp"

namespace hmi::platform::settings {
namespace {

constexpr char kPreviewRelativePath[] = "assets/sounds/volume_preview.wav";

}  // namespace

MemoryLevelSettings::MemoryLevelSettings(
    hmi::platform::windows::SdlLvglPort& display,
    hmi::audio::AudioPlaybackPort playback) noexcept
    : display_(&display), playback_(playback) {}

MemoryLevelSettings::~MemoryLevelSettings() {
  stop_preview_file();
}

hmi::settings::LevelSettingsPort MemoryLevelSettings::port() noexcept {
  return {this, load, save_brightness, save_volume, apply_brightness,
          apply_volume, play_preview, stop_preview};
}

std::uint8_t MemoryLevelSettings::applied_brightness() const noexcept {
  return applied_brightness_;
}

std::uint8_t MemoryLevelSettings::applied_volume() const noexcept {
  return applied_volume_;
}

unsigned MemoryLevelSettings::preview_count() const noexcept {
  return preview_count_;
}

bool MemoryLevelSettings::load(void* context, std::uint8_t& brightness,
                               std::uint8_t& volume, bool& has_brightness,
                               bool& has_volume) {
  auto& self = *static_cast<MemoryLevelSettings*>(context);
  brightness = self.stored_brightness_;
  volume = self.stored_volume_;
  has_brightness = self.has_brightness_;
  has_volume = self.has_volume_;
  return true;
}

bool MemoryLevelSettings::save_brightness(void* context, std::uint8_t value) {
  auto& self = *static_cast<MemoryLevelSettings*>(context);
  self.stored_brightness_ = value;
  self.has_brightness_ = true;
  return true;
}

bool MemoryLevelSettings::save_volume(void* context, std::uint8_t value) {
  auto& self = *static_cast<MemoryLevelSettings*>(context);
  self.stored_volume_ = value;
  self.has_volume_ = true;
  return true;
}

bool MemoryLevelSettings::apply_brightness(void* context,
                                           std::uint8_t value) {
  auto& self = *static_cast<MemoryLevelSettings*>(context);
  if (self.display_ != nullptr && !self.display_->set_brightness_level(value)) {
    return false;
  }
  self.applied_brightness_ = value;
  return true;
}

bool MemoryLevelSettings::apply_volume(void* context, std::uint8_t value) {
  auto& self = *static_cast<MemoryLevelSettings*>(context);
  if (!self.playback_.valid() ||
      !self.playback_.set_volume(self.playback_.context,
                                 static_cast<std::uint8_t>(value * 10U))) {
    return false;
  }
  self.applied_volume_ = value;
  return true;
}

bool MemoryLevelSettings::play_preview(void* context) {
  auto& self = *static_cast<MemoryLevelSettings*>(context);
  const bool played = self.display_ == nullptr || self.play_preview_file();
  if (played) {
    ++self.preview_count_;
  }
  return played;
}

bool MemoryLevelSettings::stop_preview(void* context) {
  auto& self = *static_cast<MemoryLevelSettings*>(context);
  return self.display_ == nullptr || self.stop_preview_file();
}

bool MemoryLevelSettings::play_preview_file() noexcept {
  return playback_.valid() &&
         playback_.play(playback_.context, kPreviewRelativePath);
}

bool MemoryLevelSettings::stop_preview_file() noexcept {
  return playback_.valid() && playback_.stop(playback_.context);
}

}  // namespace hmi::platform::settings
