#pragma once

#include <cstdint>

#include <lvgl.h>

#include "hmi/i18n/locale.hpp"
#include "hmi/storage/storage_access_controller.hpp"
#include "hmi/ui/theme.hpp"

namespace hmi::samples::suite {

struct StorageAccessLockSnapshot {
  hmi::storage::StorageAccessState state =
      hmi::storage::StorageAccessState::unavailable;
  hmi::storage::StorageAccessError error =
      hmi::storage::StorageAccessError::none;
  bool active = false;
  bool return_confirmation = false;
};

/**
 * USB MSC가 SD를 소유하는 동안 sample-suite 전체 입력을 차단하는 system overlay다.
 *
 * SD가 unmount된 상태에서도 안전하도록 catalog 문구와 firmware 고정 font만 사용한다.
 * 명시적 복귀와 remount 재시도 외에는 어떤 navigation intent도 만들지 않는다.
 */
class StorageAccessLock {
 public:
  StorageAccessLock() = default;
  StorageAccessLock(const StorageAccessLock&) = delete;
  StorageAccessLock& operator=(const StorageAccessLock&) = delete;

  bool create(lv_obj_t* parent, hmi::storage::StorageAccessController& controller,
              hmi::i18n::Locale locale, hmi::ui::UiTheme& theme) noexcept;
  void destroy() noexcept;
  void refresh() noexcept;
  [[nodiscard]] StorageAccessLockSnapshot snapshot() const noexcept;
  ~StorageAccessLock();

 private:
  static void handle_action(lv_event_t* event);
  void update_view(const hmi::storage::StorageAccessSnapshot& snapshot) noexcept;
  bool request_action() noexcept;

  hmi::storage::StorageAccessController* controller_ = nullptr;
  hmi::ui::UiTheme* theme_ = nullptr;
  hmi::i18n::Locale locale_ = hmi::i18n::Locale::ko_kr;
  hmi::storage::StorageAccessState visible_state_ =
      hmi::storage::StorageAccessState::unavailable;
  hmi::storage::StorageAccessError visible_error_ =
      hmi::storage::StorageAccessError::none;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* card_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* body_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* action_ = nullptr;
  bool return_confirmation_ = false;
};

static_assert(sizeof(StorageAccessLock) <= 128U,
              "storage lock must keep only bounded scalar state and LVGL handles");

}  // namespace hmi::samples::suite
