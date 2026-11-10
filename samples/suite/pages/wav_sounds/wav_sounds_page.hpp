#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "generated/wav_catalog.hpp"
#include "hmi/audio/playback.hpp"
#include "page_contract.hpp"

namespace hmi::samples::suite {

struct WavSoundsPageSnapshot {
  hmi::audio::PlaybackState state = hmi::audio::PlaybackState::unavailable;
  hmi::audio::PlaybackError error = hmi::audio::PlaybackError::unavailable;
  std::uint32_t elapsed_ms = 0U;
  std::uint32_t total_ms = 0U;
  std::uint32_t remaining_ms = 0U;
  std::uint32_t revision = 0U;
  std::uint16_t progress_permille = 0U;
  std::uint8_t selected_index = 0xFFU;
  bool primary_enabled = false;
  bool stop_enabled = false;
};

/**
 * 등록 WAV catalog와 공통 playback controller를 2열 player 화면으로 연결한다.
 *
 * catalog와 playback port는 비소유이며 page보다 오래 살아야 한다. page는 LVGL timer와
 * widget만 소유하고 destroy에서 timer를 제거한 뒤 재생을 중지한다.
 */
class WavSoundsPage final : public SamplePage {
 public:
  WavSoundsPage() = default;
  WavSoundsPage(const WavSoundsPage&) = delete;
  WavSoundsPage& operator=(const WavSoundsPage&) = delete;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override { return {}; }
  [[nodiscard]] WavSoundsPageSnapshot playback_snapshot() const noexcept {
    return snapshot_;
  }
  bool play_index(std::size_t index) noexcept;
  bool pause() noexcept;
  bool resume() noexcept;
  bool stop() noexcept;
  void refresh() noexcept;
  ~WavSoundsPage() override;

 private:
  static void handle_event(lv_event_t* event);
  static void handle_locale_ko_event(lv_event_t* event);
  static void handle_locale_en_event(lv_event_t* event);
  static void handle_theme_event(lv_event_t* event);
  static void handle_timer(lv_timer_t* timer) noexcept;
  void update_view() noexcept;
  void reset_widgets() noexcept;

  PageContext context_{};
  hmi::audio::AudioPlayback playback_{};
  WavSoundsPageSnapshot snapshot_{};
  lv_obj_t* root_ = nullptr;
  lv_obj_t* back_button_ = nullptr;
  std::array<lv_obj_t*, generated::kWavCatalog.size()> rows_{};
  std::array<lv_obj_t*, generated::kWavCatalog.size()> row_status_{};
  lv_obj_t* selected_title_ = nullptr;
  lv_obj_t* selected_description_ = nullptr;
  lv_obj_t* selected_format_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
  lv_obj_t* progress_bar_ = nullptr;
  lv_obj_t* elapsed_value_ = nullptr;
  lv_obj_t* total_value_ = nullptr;
  lv_obj_t* remaining_value_ = nullptr;
  lv_obj_t* play_button_ = nullptr;
  lv_obj_t* play_button_label_ = nullptr;
  lv_obj_t* stop_button_ = nullptr;
  lv_timer_t* timer_ = nullptr;
};

static_assert(sizeof(WavSoundsPage) <= 416U,
              "WAV page must keep only bounded scalar state and widget handles");

}  // namespace hmi::samples::suite
