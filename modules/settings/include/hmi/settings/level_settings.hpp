#pragma once

#include <cstdint>

#include "hmi/settings/level_settings_port.hpp"

namespace hmi::settings {

enum class LevelSettingsResult : std::uint8_t {
  ok,
  unchanged,
  invalid,
  unavailable,
  apply_failed,
  save_failed,
  preview_unavailable,
};

/**
 * 밝기 1~10과 음량 0~10의 저장·적용 순서를 관리하는 공통 model이다.
 *
 * 플랫폼 자원은 소유하지 않으며 host가 주입한 LevelSettingsPort는 이 객체보다 오래 산다.
 * 작은 scalar 상태만 보관해 Windows와 HMI에서 같은 실패/복원 규칙을 사용한다.
 */
class LevelSettings {
 public:
  LevelSettings() = default;
  ~LevelSettings() { end(); }

  LevelSettings(const LevelSettings&) = delete;
  LevelSettings& operator=(const LevelSettings&) = delete;

  bool begin(LevelSettingsPort port, std::uint8_t initial_brightness = 10U,
             std::uint8_t initial_volume = 10U) noexcept;
  void end() noexcept;
  LevelSettingsResult set_brightness(std::uint8_t level) noexcept;
  LevelSettingsResult set_volume(std::uint8_t level) noexcept;
  LevelSettingsResult play_preview() noexcept;
  void retry_volume() noexcept;

  [[nodiscard]] std::uint8_t brightness() const noexcept { return brightness_; }
  [[nodiscard]] std::uint8_t volume() const noexcept { return volume_; }
  [[nodiscard]] bool volume_pending() const noexcept { return volume_pending_; }
  [[nodiscard]] bool ready() const noexcept { return ready_; }

 private:
  LevelSettingsPort port_{};
  std::uint8_t brightness_ = 10U;
  std::uint8_t volume_ = 10U;
  bool ready_ = false;
  bool volume_pending_ = false;
};

static_assert(sizeof(LevelSettings) <= 80U,
              "LevelSettings must remain a small scalar model");

}  // namespace hmi::settings
