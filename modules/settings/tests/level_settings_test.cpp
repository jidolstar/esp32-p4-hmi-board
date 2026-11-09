#include <cassert>
#include <cstdint>

#include "hmi/settings/level_settings.hpp"

namespace {

struct FakePort {
  std::uint8_t stored_brightness = 0U;
  std::uint8_t stored_volume = 0U;
  std::uint8_t applied_brightness = 0U;
  std::uint8_t applied_volume = 0U;
  unsigned preview_count = 0U;
  unsigned stop_count = 0U;
  bool has_brightness = false;
  bool has_volume = false;
  bool load_ok = true;
  bool save_brightness_ok = true;
  bool save_volume_ok = true;
  bool apply_brightness_ok = true;
  bool apply_volume_ok = true;
  bool preview_ok = true;
  bool stop_ok = true;
};

bool load(void* context, std::uint8_t& brightness, std::uint8_t& volume,
          bool& has_brightness, bool& has_volume) {
  auto& fake = *static_cast<FakePort*>(context);
  brightness = fake.stored_brightness;
  volume = fake.stored_volume;
  has_brightness = fake.has_brightness;
  has_volume = fake.has_volume;
  return fake.load_ok;
}

bool save_brightness(void* context, std::uint8_t value) {
  auto& fake = *static_cast<FakePort*>(context);
  if (!fake.save_brightness_ok) {
    return false;
  }
  fake.stored_brightness = value;
  fake.has_brightness = true;
  return true;
}

bool save_volume(void* context, std::uint8_t value) {
  auto& fake = *static_cast<FakePort*>(context);
  if (!fake.save_volume_ok) {
    return false;
  }
  fake.stored_volume = value;
  fake.has_volume = true;
  return true;
}

bool apply_brightness(void* context, std::uint8_t value) {
  auto& fake = *static_cast<FakePort*>(context);
  if (!fake.apply_brightness_ok) {
    return false;
  }
  fake.applied_brightness = value;
  return true;
}

bool apply_volume(void* context, std::uint8_t value) {
  auto& fake = *static_cast<FakePort*>(context);
  if (!fake.apply_volume_ok) {
    return false;
  }
  fake.applied_volume = value;
  return true;
}

bool play_preview(void* context) {
  auto& fake = *static_cast<FakePort*>(context);
  ++fake.preview_count;
  return fake.preview_ok;
}

bool stop_preview(void* context) {
  auto& fake = *static_cast<FakePort*>(context);
  ++fake.stop_count;
  return fake.stop_ok;
}

hmi::settings::LevelSettingsPort port(FakePort& fake) {
  return {&fake, load, save_brightness, save_volume, apply_brightness,
          apply_volume, play_preview, stop_preview};
}

}  // namespace

int main() {
  using hmi::settings::LevelSettings;
  using hmi::settings::LevelSettingsResult;

  FakePort fake;
  LevelSettings settings;
  assert(settings.begin(port(fake), 7U, 4U));
  assert(settings.ready());
  assert(settings.brightness() == 7U && settings.volume() == 4U);
  assert(fake.stored_brightness == 7U && fake.stored_volume == 4U);
  assert(fake.applied_brightness == 7U && fake.applied_volume == 4U);

  assert(settings.set_brightness(0U) == LevelSettingsResult::invalid);
  assert(settings.set_brightness(9U) == LevelSettingsResult::ok);
  assert(fake.stored_brightness == 9U && fake.applied_brightness == 9U);

  fake.save_brightness_ok = false;
  assert(settings.set_brightness(8U) == LevelSettingsResult::save_failed);
  assert(settings.brightness() == 9U && fake.applied_brightness == 9U);
  fake.save_brightness_ok = true;

  assert(settings.set_volume(6U) == LevelSettingsResult::ok);
  assert(fake.preview_count == 1U && fake.applied_volume == 6U);
  assert(settings.play_preview() == LevelSettingsResult::ok);
  assert(fake.preview_count == 2U);
  assert(settings.set_volume(0U) == LevelSettingsResult::ok);
  assert(fake.stop_count == 1U && fake.applied_volume == 0U);

  fake.apply_volume_ok = false;
  assert(settings.set_volume(3U) == LevelSettingsResult::apply_failed);
  assert(settings.volume() == 3U && settings.volume_pending());
  fake.apply_volume_ok = true;
  settings.retry_volume();
  assert(!settings.volume_pending() && fake.applied_volume == 3U);
  settings.end();
  assert(!settings.ready() && fake.stop_count == 2U);
  settings.end();
  assert(fake.stop_count == 2U);

  FakePort invalid;
  invalid.has_brightness = true;
  invalid.has_volume = true;
  invalid.stored_brightness = 0U;
  invalid.stored_volume = 99U;
  LevelSettings bounded;
  assert(bounded.begin(port(invalid)));
  assert(bounded.brightness() == 10U && bounded.volume() == 10U);

  FakePort unavailable;
  unavailable.load_ok = false;
  LevelSettings failed;
  assert(!failed.begin(port(unavailable)));
  assert(failed.set_volume(1U) == LevelSettingsResult::unavailable);
  return 0;
}
