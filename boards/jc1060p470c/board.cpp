#include "board.hpp"

#include <Arduino.h>
#include <lvgl.h>
#include <src/extra/libs/fsdrv/lv_fsdrv.h>

#include <new>

#include "config.hpp"
#if HMI_PROFILE_HAS_ASSETS
#include "internal/asset_usb_updater.hpp"
#endif
#if HMI_PROFILE_HAS_STORAGE
#include "internal/storage_msc_transition.hpp"
#endif
#include "log.hpp"

namespace hmi::boards::jc1060p470c {

Board::Board()
#if HMI_PROFILE_HAS_SETTINGS
    : touch_(touch_i2c_), storage_facade_(storage_),
      audio_service_(storage_facade_, touch_i2c_),
      audio_(audio_service_) {}
#else
    : touch_(touch_i2c_), storage_facade_(storage_) {}
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
  if (!storage_facade_.ready() && !storage_.begin()) {
    return false;
  }

  if (lvgl_storage_ready_) {
    return true;
  }

  // lv_font_load의 A: 경로가 Arduino VFS의 /sdcard mount를 사용하도록 연결한다.
  lv_fs_stdio_init();
  lvgl_storage_ready_ = true;
  HMI_LOG_INFO("[board] LVGL storage driver ready letter=A\r\n");
  return true;
}

bool Board::storage_ready() const noexcept { return storage_facade_.ready(); }

UsbMscEnterResult Board::enter_usb_msc() noexcept {
#if HMI_PROFILE_HAS_STORAGE
  return internal::enter_storage_usb_msc(storage_, usb_msc_);
#else
  return UsbMscEnterResult::unavailable;
#endif
}

UsbMscExitResult Board::exit_usb_msc(
    std::uint32_t drain_timeout_ms) noexcept {
#if HMI_PROFILE_HAS_STORAGE
  return internal::exit_storage_usb_msc(storage_, usb_msc_,
                                        drain_timeout_ms);
#else
  (void)drain_timeout_ms;
  return UsbMscExitResult::not_active;
#endif
}

bool Board::retry_storage_mount() noexcept {
  return !storage_facade_.ready() && storage_.begin();
}

UsbMscStatus Board::usb_msc_status() const noexcept {
#if HMI_PROFILE_HAS_STORAGE
  const auto status = usb_msc_.snapshot();
  return {status.active,
          status.media_present,
          status.host_seen,
          status.host_active,
          status.host_ejected,
          status.usb_started,
          status.usb_stopped,
          status.suspended,
          status.io_error,
          status.in_flight,
          status.event_revision};
#else
  return {};
#endif
}

bool Board::enable_usb_asset_updates(const char* profile_id,
                                     const char* storage_root) {
#if HMI_PROFILE_HAS_ASSETS
  if (!asset_usb_updater_) {
    asset_usb_updater_.reset(
        new (std::nothrow) internal::AssetUsbUpdater(storage_facade_));
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
