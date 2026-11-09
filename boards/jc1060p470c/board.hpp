#pragma once

#include <memory>

#include <Wire.h>

#include "asset_update_status.hpp"
#include "internal/mipi_dsi_display.hpp"
#include "internal/sd_storage.hpp"
#include "internal/lvgl_port.hpp"
#include "internal/touch_controller.hpp"
#include "storage.hpp"
#include "touch_point.hpp"

#ifndef HMI_PROFILE_HAS_SETTINGS
#define HMI_PROFILE_HAS_SETTINGS 0
#endif

#if HMI_PROFILE_HAS_SETTINGS
#include "audio.hpp"
#include "internal/audio_service.hpp"
#endif

#ifndef HMI_PROFILE_HAS_ASSETS
#define HMI_PROFILE_HAS_ASSETS 0
#endif

#ifndef HMI_PROFILE_HAS_STORAGE
#define HMI_PROFILE_HAS_STORAGE 0
#endif

#if HMI_PROFILE_HAS_STORAGE
#include "internal/usb_msc_driver.hpp"
#endif

namespace hmi::boards::jc1060p470c {

namespace internal {
class AssetUsbUpdater;
}

/**
 * JC1060P470C의 display, touch와 선택 profile의 장치 서비스를 소유하는 hardware facade다.
 *
 * Host는 이 public API만 사용한다. 내부 MIPI-DSI, GT911, I2C 객체의 lifetime은
 * Board와 같고 앱에는 노출되지 않는다. 제품 UI와 장치 서비스는 소유하지 않는다.
 */
class Board {
 public:
  Board();
  ~Board();
  Board(const Board &) = delete;
  Board &operator=(const Board &) = delete;

  /** LCD, touch, LVGL display/input port를 올바른 순서로 초기화한다. */
  bool begin();

  /** Arduino loop에서 LVGL tick과 timer handler를 실행한다. */
  void tick();

  /** 외부 자산용 TF 카드를 mount하고 LVGL 파일 드라이버를 한 번 등록한다. */
  bool begin_storage();

  /** 현재 profile root만 갱신하는 USB CDC 자산 수신기를 시작한다. */
  bool enable_usb_asset_updates(const char* profile_id,
                                const char* storage_root);

  /** USB 자산 갱신 진행 상태를 UI 독립 값으로 반환한다. */
  [[nodiscard]] AssetUpdateStatus asset_update_status() const noexcept;

  /** host가 완료 화면을 표시한 뒤 장치를 software reset한다. */
  void restart() noexcept;

  /** 진단에서 사용할 마지막 touch 좌표 snapshot을 반환한다. */
  TouchPoint last_touch() const;

  /** UI 정책과 저장을 소유하지 않고 밝기 단계 1~10만 실제 PWM에 적용한다. */
  bool set_backlight_level(std::uint8_t level);

#if HMI_PROFILE_HAS_SETTINGS
  /** Board가 소유한 WAV/audio service의 비소유 facade를 반환한다. */
  Audio& audio() noexcept { return audio_; }
#endif

  /** TF 카드가 FAT filesystem으로 mount됐는지 반환한다. */
  [[nodiscard]] bool storage_ready() const noexcept;

  /** SD 구현 세부사항을 숨긴 Board-owned 저장소 facade를 반환한다. */
  Storage& storage() noexcept { return storage_facade_; }

  /** file lease가 없을 때 SD를 USB MSC raw owner로 전환한다. */
  UsbMscEnterResult enter_usb_msc() noexcept;

  /** 새 USB I/O를 차단하고 callback drain 뒤 SD를 HMI filesystem으로 재마운트한다. */
  UsbMscExitResult exit_usb_msc(std::uint32_t drain_timeout_ms) noexcept;

  /** remount 실패 뒤 SD filesystem mount만 다시 시도한다. */
  bool retry_storage_mount() noexcept;

  /** USB callback이 기록한 작은 상태 snapshot을 반환한다. */
  [[nodiscard]] UsbMscStatus usb_msc_status() const noexcept;

 private:
  bool begin_hardware();

  TwoWire touch_i2c_{1};
  internal::MipiDsiDisplay display_;
  internal::TouchController touch_;
  internal::SdStorage storage_;
  Storage storage_facade_;
#if HMI_PROFILE_HAS_STORAGE
  internal::UsbMscDriver usb_msc_;
#endif
#if HMI_PROFILE_HAS_SETTINGS
  internal::AudioService audio_service_;
  Audio audio_;
#endif
#if HMI_PROFILE_HAS_ASSETS
  std::unique_ptr<internal::AssetUsbUpdater> asset_usb_updater_{};
#endif
  internal::LvglPort lvgl_;
  bool lvgl_storage_ready_ = false;
};

}  // namespace hmi::boards::jc1060p470c
