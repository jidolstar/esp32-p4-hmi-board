#include "board.hpp"

#include <Arduino.h>
#include <lvgl.h>
#include <src/extra/libs/fsdrv/lv_fsdrv.h>

#include <new>

#include "config.hpp"
#if HMI_PROFILE_HAS_ASSETS
#include "internal/asset_usb_updater.hpp"
#endif
#include "log.hpp"

namespace hmi::boards::jc1060p470c {

Board::Board()
#if HMI_PROFILE_HAS_SETTINGS
    : touch_(touch_i2c_), audio_service_(storage_, touch_i2c_),
      audio_(audio_service_) {}
#else
    : touch_(touch_i2c_) {}
#endif

Board::~Board() = default;

bool Board::begin() {
  if (!begin_hardware()) {
    return false;
  }

  lv_init();
  if (!lvgl_.attach_display(display_)) {
    HMI_LOG_ERROR("[board] LVGL display attach failed\r\n");
    return false;
  }
  if (!lvgl_.attach_touch(touch_)) {
    HMI_LOG_ERROR("[board] LVGL touch attach failed\r\n");
    return false;
  }

  HMI_LOG_INFO("[board] LVGL ready\r\n");
  return true;
}

bool Board::begin_hardware() {
  HMI_LOG_INFO("[board] begin name=%s\r\n", config::kName);

  if (!touch_i2c_.begin(config::kTouchSda, config::kTouchScl,
                        config::kTouchI2cFrequency)) {
    HMI_LOG_ERROR("[board] touch I2C init failed\r\n");
    return false;
  }
  if (!display_.begin()) {
    HMI_LOG_ERROR("[board] display init failed\r\n");
    return false;
  }
  if (!touch_.begin()) {
    HMI_LOG_ERROR("[board] touch init failed\r\n");
    return false;
  }

#if HMI_PROFILE_HAS_SETTINGS
  // 오디오 실패는 display/touch 부팅을 막지 않으며 page에서 unavailable로 드러낸다.
  if (!audio_service_.begin()) {
    HMI_LOG_ERROR("[audio] worker unavailable\r\n");
  }
#endif

  HMI_LOG_INFO("[board] hardware ready\r\n");
  return true;
}

TouchPoint Board::last_touch() const { return touch_.last_touch(); }

bool Board::set_backlight_level(std::uint8_t level) {
  return display_.set_backlight_level(level);
}

bool Board::begin_storage() {
  if (lvgl_storage_ready_) {
    return true;
  }
  if (!storage_.begin()) {
    return false;
  }

  // lv_font_load의 A: 경로가 Arduino VFS의 /sdcard mount를 사용하도록 연결한다.
  lv_fs_stdio_init();
  lvgl_storage_ready_ = true;
  HMI_LOG_INFO("[board] LVGL storage driver ready letter=A\r\n");
  return true;
}

bool Board::storage_ready() const noexcept { return storage_.ready(); }

bool Board::enable_usb_asset_updates(const char* profile_id,
                                     const char* storage_root) {
#if HMI_PROFILE_HAS_ASSETS
  if (!asset_usb_updater_) {
    asset_usb_updater_.reset(
        new (std::nothrow) internal::AssetUsbUpdater(storage_));
  }
  return asset_usb_updater_ != nullptr &&
         asset_usb_updater_->begin(profile_id, storage_root);
#else
  (void)profile_id;
  (void)storage_root;
  return false;
#endif
}

AssetUpdateStatus Board::asset_update_status() const noexcept {
#if HMI_PROFILE_HAS_ASSETS
  return asset_usb_updater_ == nullptr ? AssetUpdateStatus{}
                                       : asset_usb_updater_->status();
#else
  return {};
#endif
}

void Board::restart() noexcept {
  HMI_LOG_INFO("[board] software restart requested\r\n");
  ESP.restart();
}

void Board::tick() {
  // CDC 수신량을 제한해 같은 loop의 LVGL timer 처리를 계속 보장한다.
#if HMI_PROFILE_HAS_ASSETS
  if (asset_usb_updater_ != nullptr) {
    asset_usb_updater_->tick();
  }
#endif
  lvgl_.tick(millis());
}

}  // namespace hmi::boards::jc1060p470c
