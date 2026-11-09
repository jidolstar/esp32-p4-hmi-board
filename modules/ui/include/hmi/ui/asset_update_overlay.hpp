#pragma once

#include <cstdint>

#include <lvgl.h>

namespace hmi::ui {

enum class AssetUpdateViewState : std::uint8_t {
  preparing,
  receiving,
  completed,
  failed,
};

enum class AssetUpdatePreparationStep : std::uint8_t {
  none,
  fonts,
  assets,
};

struct AssetUpdateViewModel {
  AssetUpdateViewState state = AssetUpdateViewState::receiving;
  AssetUpdatePreparationStep preparation = AssetUpdatePreparationStep::none;
  std::uint16_t file_index = 0U;
  std::uint16_t file_count = 0U;
  std::uint32_t bytes_received = 0U;
  std::uint32_t total_bytes = 0U;
};

/**
 * SD 자산과 상품 theme 없이 동작하는 HMI 자산 갱신 전용 overlay다.
 *
 * 내장 Montserrat와 고정 색상만 사용하므로 active 자산 directory가 교체되는 동안에도
 * 표시할 수 있다. LVGL top layer가 widget을 소유하며 이 객체는 handle만 보관한다.
 */
class AssetUpdateOverlay {
 public:
  AssetUpdateOverlay() = default;
  AssetUpdateOverlay(const AssetUpdateOverlay&) = delete;
  AssetUpdateOverlay& operator=(const AssetUpdateOverlay&) = delete;
  ~AssetUpdateOverlay();

  bool show() noexcept;
  void update(const AssetUpdateViewModel& model) noexcept;
  void destroy() noexcept;

  [[nodiscard]] bool visible() const noexcept { return root_ != nullptr; }
  [[nodiscard]] std::uint8_t progress_percent() const noexcept {
    return progress_percent_;
  }
  [[nodiscard]] AssetUpdateViewState state() const noexcept { return state_; }

 private:
  static void handle_delete(lv_event_t* event) noexcept;
  static void set_preparation_progress(void* bar, std::int32_t value) noexcept;

  lv_obj_t* root_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
  lv_obj_t* progress_bar_ = nullptr;
  lv_obj_t* percent_label_ = nullptr;
  lv_obj_t* detail_label_ = nullptr;
  AssetUpdateViewModel last_{};
  AssetUpdateViewState state_ = AssetUpdateViewState::receiving;
  std::uint8_t progress_percent_ = 0U;
  bool has_last_ = false;
};

static_assert(sizeof(AssetUpdateOverlay) <= 96U,
              "Asset update overlay must retain only LVGL handles and scalars");

}  // namespace hmi::ui
