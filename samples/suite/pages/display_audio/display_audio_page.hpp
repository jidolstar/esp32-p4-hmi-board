#pragma once

#include <cstdint>

#include <lvgl.h>

#include "hmi/settings/level_settings.hpp"
#include "page_contract.hpp"

namespace hmi::samples::suite {

/**
 * 화면 밝기와 시스템 음량을 한 화면에서 조절하는 설정 page다.
 *
 * 장치와 저장소는 LevelSettingsPort 뒤에 숨기고, page는 widget 수명과 사용자 피드백만
 * 소유한다. destroy 시 preview를 먼저 멈춘 뒤 widget을 해제한다.
 */
class DisplayAudioPage final : public SamplePage {
 public:
  DisplayAudioPage() = default;
  DisplayAudioPage(const DisplayAudioPage&) = delete;
  DisplayAudioPage& operator=(const DisplayAudioPage&) = delete;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override;
  ~DisplayAudioPage() override;

  hmi::settings::LevelSettingsResult set_brightness(std::uint8_t level) noexcept;
  hmi::settings::LevelSettingsResult set_volume(std::uint8_t level) noexcept;
  hmi::settings::LevelSettingsResult play_preview() noexcept;

 private:
  static void handle_event(lv_event_t* event);
  static void handle_locale_ko_event(lv_event_t* event);
  static void handle_locale_en_event(lv_event_t* event);
  static void handle_theme_event(lv_event_t* event);
  void accept_result(hmi::settings::LevelSettingsResult result) noexcept;
  void update_view() noexcept;

  PageContext context_{};
  PageSnapshot state_{};
  hmi::settings::LevelSettings settings_{};
  lv_obj_t* root_ = nullptr;
  lv_obj_t* back_button_ = nullptr;
  lv_obj_t* brightness_slider_ = nullptr;
  lv_obj_t* brightness_value_ = nullptr;
  lv_obj_t* brightness_down_ = nullptr;
  lv_obj_t* brightness_up_ = nullptr;
  lv_obj_t* volume_slider_ = nullptr;
  lv_obj_t* volume_value_ = nullptr;
  lv_obj_t* volume_down_ = nullptr;
  lv_obj_t* volume_up_ = nullptr;
  lv_obj_t* preview_button_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
};

}  // namespace hmi::samples::suite
