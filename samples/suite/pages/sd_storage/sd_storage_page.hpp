#pragma once

#include <cstdint>

#include <lvgl.h>

#include "hmi/storage/storage_access_controller.hpp"
#include "page_contract.hpp"

namespace hmi::samples::suite {

struct SdStoragePageSnapshot {
  hmi::storage::StorageAccessState state =
      hmi::storage::StorageAccessState::unavailable;
  hmi::storage::StorageAccessError error =
      hmi::storage::StorageAccessError::none;
  hmi::storage::StorageOwner owner = hmi::storage::StorageOwner::none;
  std::uint32_t generation = 0U;
  std::uint32_t rescan_revision = 0U;
  std::uint8_t used_percent = 0U;
  bool media_present = false;
  bool connect_enabled = false;
};

/** SD mount/용량/owner를 표시하고 USB MSC 전환 intent만 controller에 전달하는 page다. */
class SdStoragePage final : public SamplePage {
 public:
  SdStoragePage() = default;
  SdStoragePage(const SdStoragePage&) = delete;
  SdStoragePage& operator=(const SdStoragePage&) = delete;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override { return {}; }
  [[nodiscard]] SdStoragePageSnapshot storage_snapshot() const noexcept;
  bool request_pc_access() noexcept;
  bool retry_media() noexcept;
  void refresh() noexcept;
  ~SdStoragePage() override;

 private:
  static void handle_event(lv_event_t* event);
  static void handle_locale_ko_event(lv_event_t* event);
  static void handle_locale_en_event(lv_event_t* event);
  static void handle_theme_event(lv_event_t* event);
  static void handle_timer(lv_timer_t* timer) noexcept;
  void update_values(const hmi::storage::StorageAccessSnapshot& value) noexcept;

  PageContext context_{};
  SdStoragePageSnapshot snapshot_{};
  lv_obj_t* root_ = nullptr;
  lv_obj_t* back_button_ = nullptr;
  lv_obj_t* connect_button_ = nullptr;
  lv_obj_t* status_value_ = nullptr;
  lv_obj_t* owner_value_ = nullptr;
  lv_obj_t* filesystem_value_ = nullptr;
  lv_obj_t* total_value_ = nullptr;
  lv_obj_t* used_value_ = nullptr;
  lv_obj_t* free_value_ = nullptr;
  lv_obj_t* usage_bar_ = nullptr;
  lv_obj_t* message_ = nullptr;
  lv_timer_t* timer_ = nullptr;
};

static_assert(sizeof(SdStoragePage) <= 256U,
              "SD storage page must keep only bounded scalar state and handles");

}  // namespace hmi::samples::suite
