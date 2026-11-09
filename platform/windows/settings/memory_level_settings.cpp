#include "settings/memory_level_settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "sdl_lvgl_port.hpp"

namespace hmi::platform::settings {
namespace {

constexpr char kPreviewRelativePath[] = "assets/sounds/volume_preview.wav";

bool same_audio_format(const SDL_AudioSpec& left,
                       const SDL_AudioSpec& right) noexcept {
  return left.freq == right.freq && left.format == right.format &&
         left.channels == right.channels;
}

}  // namespace

MemoryLevelSettings::MemoryLevelSettings(
    hmi::platform::windows::SdlLvglPort& display,
    const char* asset_root) noexcept
    : display_(&display) {
  if (asset_root == nullptr || asset_root[0] == '\0') {
    return;
  }
  // LVGL stdio driver의 `A:` prefix는 SDL_LoadWAV가 이해하지 못하므로 제거한다.
  const char* filesystem_root =
      asset_root[0] != '\0' && asset_root[1] == ':' ? asset_root + 2 : asset_root;
  const int written = std::snprintf(preview_path_.data(), preview_path_.size(),
                                    "%s%s", filesystem_root,
                                    kPreviewRelativePath);
  preview_path_valid_ =
      written > 0 && static_cast<std::size_t>(written) < preview_path_.size();
}

MemoryLevelSettings::~MemoryLevelSettings() {
  stop_preview_file();
  if (audio_device_ != 0U) {
    SDL_CloseAudioDevice(audio_device_);
    audio_device_ = 0U;
  }
  if (audio_subsystem_owned_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
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
  static_cast<MemoryLevelSettings*>(context)->applied_volume_ = value;
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

bool MemoryLevelSettings::open_audio(const SDL_AudioSpec& source_spec) noexcept {
  if (audio_device_ != 0U && same_audio_format(audio_spec_, source_spec)) {
    return true;
  }
  if (audio_device_ != 0U) {
    SDL_CloseAudioDevice(audio_device_);
    audio_device_ = 0U;
  }
  if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      std::fprintf(stderr, "SDL audio init: %s\n", SDL_GetError());
      return false;
    }
    audio_subsystem_owned_ = true;
  }
  audio_spec_ = source_spec;
  audio_spec_.callback = nullptr;
  audio_spec_.userdata = nullptr;
  audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &audio_spec_, nullptr, 0);
  if (audio_device_ == 0U) {
    std::fprintf(stderr, "SDL audio device: %s\n", SDL_GetError());
    return false;
  }
  return true;
}

bool MemoryLevelSettings::play_preview_file() noexcept {
  if (!preview_path_valid_) {
    return false;
  }
  SDL_AudioSpec source_spec{};
  std::uint8_t* source = nullptr;
  std::uint32_t source_size = 0U;
  if (SDL_LoadWAV(preview_path_.data(), &source_spec, &source, &source_size) ==
      nullptr) {
    std::fprintf(stderr, "SDL_LoadWAV(%s): %s\n", preview_path_.data(),
                 SDL_GetError());
    return false;
  }
  if (!open_audio(source_spec)) {
    SDL_FreeWAV(source);
    return false;
  }

  std::vector<std::uint8_t> mixed(source_size, source_spec.silence);
  const int mix_volume = static_cast<int>(
      std::min<std::uint8_t>(applied_volume_, 10U) * SDL_MIX_MAXVOLUME / 10U);
  SDL_MixAudioFormat(mixed.data(), source, source_spec.format, source_size,
                     mix_volume);
  SDL_FreeWAV(source);

  SDL_ClearQueuedAudio(audio_device_);
  if (SDL_QueueAudio(audio_device_, mixed.data(), source_size) != 0) {
    std::fprintf(stderr, "SDL_QueueAudio: %s\n", SDL_GetError());
    return false;
  }
  SDL_PauseAudioDevice(audio_device_, 0);
  return true;
}

bool MemoryLevelSettings::stop_preview_file() noexcept {
  if (audio_device_ != 0U) {
    SDL_ClearQueuedAudio(audio_device_);
    SDL_PauseAudioDevice(audio_device_, 1);
  }
  return true;
}

}  // namespace hmi::platform::settings
