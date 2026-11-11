#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/settings/level_settings_port.hpp"

namespace hmi::audio {
struct AudioPlaybackPort;
}

namespace hmi::connectivity {
class WifiApWebController;
class WifiStaRegistration;
}

namespace hmi::storage {
class StorageAccessController;
}

namespace hmi::time {
struct TimeRuntime;
}

namespace hmi::profiles {

enum class AppKey : std::uint8_t {
  backspace,
  enter,
  cursor_left,
  cursor_right,
};

struct BinaryFontPort {
  using Load = void* (*)(void* context, const char* relative_path,
                         std::uint32_t size_bytes,
                         std::uint32_t crc32) noexcept;
  using Free = void (*)(void* context, void* font) noexcept;

  void* context = nullptr;
  Load load = nullptr;
  Free free = nullptr;
};

/**
 * 선택된 앱에 host가 전달하는 최소 플랫폼 비종속 정보다.
 *
 * host_name, font port context와 주입된 runtime 포인터는 앱보다 오래 살아야 한다.
 * font port는 opaque binary font handle만 전달하며 display, touch, Board 내부 객체는
 * 경계를 통과하지 않는다. 앱은 host runtime보다 먼저 정리된다.
 */
struct AppContext {
  const char *host_name = nullptr;
  BinaryFontPort fonts{};
  hmi::settings::LevelSettingsPort level_settings{};
  hmi::audio::AudioPlaybackPort* audio_playback = nullptr;
  hmi::connectivity::WifiApWebController* ap_control = nullptr;
  hmi::connectivity::WifiStaRegistration* wifi_registration = nullptr;
  hmi::storage::StorageAccessController* storage_access = nullptr;
  hmi::time::TimeRuntime* time_runtime = nullptr;
};

}  // namespace hmi::profiles
