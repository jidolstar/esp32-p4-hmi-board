#include <Arduino.h>
#include <lvgl.h>

#include <array>
#include <cstdio>

#include "board.hpp"
#include "config.hpp"
#include "log.hpp"
#if HMI_PROFILE_HAS_ASSETS
#include "hmi/ui/asset_update_overlay.hpp"
#include "hmi/ui/font_repository.hpp"
#endif
#include "selected_profile.hpp"
#if HMI_PROFILE_HAS_SETTINGS
#include "settings/preferences_level_settings.hpp"
#endif
#if HMI_PROFILE_HAS_CONNECTIVITY
#include "connectivity/esp_hosted_wifi_sta.hpp"
#include "connectivity/preferences_wifi_station_settings_store.hpp"
#include "hmi/connectivity/wifi_sta_registration.hpp"
#endif
#if HMI_PROFILE_HAS_STORAGE
#include "hmi/storage/storage_access_controller.hpp"
#include "storage/board_storage_access.hpp"
#endif

namespace {

// 큰 hardware 객체가 Arduino loop stack을 사용하지 않도록 persistent lifetime으로 둔다.
hmi::boards::jc1060p470c::Board &board() {
  static hmi::boards::jc1060p470c::Board instance;
  return instance;
}

hmi::profiles::generated::SelectedProfileApp &app() {
  static hmi::profiles::generated::SelectedProfileApp instance;
  return instance;
}

bool board_ready = false;
#if HMI_PROFILE_HAS_CONNECTIVITY
bool wifi_ready = false;
#endif
#if HMI_PROFILE_HAS_STORAGE
bool storage_access_ready = false;
#endif

#if HMI_PROFILE_HAS_SETTINGS
hmi::platform::settings::PreferencesLevelSettings& level_settings() {
  static hmi::platform::settings::PreferencesLevelSettings instance(
      board(), hmi::profiles::generated::kSelectedNvsNamespace,
      hmi::profiles::generated::kSelectedAssetStorageRoot);
  return instance;
}
#endif

#if HMI_PROFILE_HAS_STORAGE
hmi::platform::hmi_host::storage::BoardStorageAccess& storage_port() {
  static hmi::platform::hmi_host::storage::BoardStorageAccess instance(board());
  return instance;
}

hmi::storage::StorageAccessController& storage_access() {
  static hmi::storage::StorageAccessController instance(storage_port());
  return instance;
}
#endif

#if HMI_PROFILE_HAS_CONNECTIVITY
hmi::platform::connectivity::EspHostedWifiSta& wifi_port() {
  static hmi::platform::connectivity::EspHostedWifiSta instance;
  return instance;
}

hmi::platform::connectivity::PreferencesWifiStationSettingsStore& wifi_store() {
  static hmi::platform::connectivity::PreferencesWifiStationSettingsStore instance(
      hmi::profiles::generated::kSelectedNvsNamespace);
  return instance;
}

hmi::connectivity::WifiStaRegistration& wifi_registration() {
  static hmi::connectivity::WifiStaRegistration instance(wifi_port(),
                                                         wifi_store());
  return instance;
}
#endif

#if HMI_PROFILE_HAS_ASSETS
hmi::ui::AssetUpdateOverlay& asset_update_overlay() {
  static hmi::ui::AssetUpdateOverlay instance;
  return instance;
}

void service_asset_update_ui() noexcept {
  if constexpr (!hmi::profiles::generated::kSelectedHasAssets) {
    return;
  }
  static std::uint32_t completion_started_ms = 0U;
  const auto status = board().asset_update_status();
  if (status.phase == hmi::boards::jc1060p470c::AssetUpdatePhase::idle) {
    return;
  }

  hmi::ui::AssetUpdateViewModel model;
  model.file_index = status.file_index;
  model.file_count = status.file_count;
  model.bytes_received = status.bytes_received;
  model.total_bytes = status.total_bytes;
  if (status.phase ==
      hmi::boards::jc1060p470c::AssetUpdatePhase::preparing) {
    model.state = hmi::ui::AssetUpdateViewState::preparing;
    if (status.preparation ==
        hmi::boards::jc1060p470c::AssetUpdatePreparationStep::fonts) {
      model.preparation = hmi::ui::AssetUpdatePreparationStep::fonts;
    } else if (status.preparation ==
               hmi::boards::jc1060p470c::AssetUpdatePreparationStep::assets) {
      model.preparation = hmi::ui::AssetUpdatePreparationStep::assets;
    }
  } else if (status.phase ==
      hmi::boards::jc1060p470c::AssetUpdatePhase::completed) {
    model.state = hmi::ui::AssetUpdateViewState::completed;
  } else if (status.phase ==
             hmi::boards::jc1060p470c::AssetUpdatePhase::failed) {
    model.state = hmi::ui::AssetUpdateViewState::failed;
  } else {
    model.state = hmi::ui::AssetUpdateViewState::receiving;
  }
  asset_update_overlay().update(model);

  if (status.phase ==
      hmi::boards::jc1060p470c::AssetUpdatePhase::completed) {
    if (completion_started_ms == 0U) {
      completion_started_ms = millis();
    } else if (static_cast<std::uint32_t>(millis() - completion_started_ms) >=
               1500U) {
      board().restart();
    }
  } else {
    // 실패 후 같은 전원 session에서 재시도하면 새 완료 시간을 다시 측정한다.
    completion_started_ms = 0U;
  }
}

void *load_font(void *, const char *relative_path, std::uint32_t size_bytes,
                std::uint32_t crc32) noexcept {
  if (relative_path == nullptr) {
    return nullptr;
  }
  std::array<char, 320U> path{};
  const int written = std::snprintf(
      path.data(), path.size(), "%s%s",
      hmi::profiles::generated::kSelectedAssetRoot, relative_path);
  if (written <= 0 || static_cast<std::size_t>(written) >= path.size()) {
    return nullptr;
  }
  return hmi::ui::load_binary_font_file(path.data(), size_bytes, crc32);
}

void free_font(void *, void *font) noexcept {
  lv_font_free(static_cast<lv_font_t *>(font));
}
#else
// 자산이 없는 profile은 UI module이나 SD 자산 runtime에 의존하지 않는다.
void service_asset_update_ui() noexcept {}
#endif

}  // namespace

void setup() {
  HMI_LOG_BEGIN(hmi::boards::jc1060p470c::config::kSerialBaud);
  board_ready = board().begin();
  if (!board_ready) {
    HMI_LOG_ERROR("[hmi-profile] board initialization failed\r\n");
    return;
  }

  hmi::profiles::AppContext app_context;
  app_context.host_name = "hmi";
#if HMI_PROFILE_HAS_SETTINGS
  app_context.level_settings = level_settings().port();
#endif
#if HMI_PROFILE_HAS_CONNECTIVITY
  wifi_ready = wifi_registration().begin(millis(), true);
  if (!wifi_ready) {
    HMI_LOG_ERROR("[hmi-profile] Wi-Fi runtime initialization failed\r\n");
  } else {
    app_context.wifi_registration = &wifi_registration();
  }
#endif
#if HMI_PROFILE_HAS_ASSETS || HMI_PROFILE_HAS_STORAGE
  {
    if (!board().begin_storage()) {
      HMI_LOG_ERROR("[hmi-profile] storage unavailable\r\n");
    }
#if HMI_PROFILE_HAS_ASSETS
    // SD mount 실패 중에도 CDC receiver를 시작해 PC에 진단 오류를 반환한다.
    if (!board().enable_usb_asset_updates(
            hmi::profiles::generated::kSelectedProfileId,
            hmi::profiles::generated::kSelectedAssetStorageRoot)) {
      HMI_LOG_ERROR("[hmi-profile] USB asset updater unavailable\r\n");
    }
#endif
  }
#endif
#if HMI_PROFILE_HAS_ASSETS
  if (board().storage_ready()) {
    app_context.fonts = {nullptr, &load_font, &free_font};
  }
#endif
#if HMI_PROFILE_HAS_STORAGE
  storage_access_ready = storage_port().begin();
  if (!storage_access_ready) {
    HMI_LOG_ERROR("[hmi-profile] storage access worker unavailable\r\n");
  } else {
    storage_access().tick(millis());
    app_context.storage_access = &storage_access();
  }
#endif
  if (!app().create(app_context)) {
    HMI_LOG_ERROR("[hmi-profile] app creation failed\r\n");
    return;
  }
  HMI_LOG_INFO("[hmi-profile] app ready\r\n");
}

void loop() {
  if (board_ready) {
    board().tick();
    service_asset_update_ui();
  }
#if HMI_PROFILE_HAS_CONNECTIVITY
  if (wifi_ready) {
    wifi_registration().tick(millis());
  }
#endif
#if HMI_PROFILE_HAS_STORAGE
  if (storage_access_ready) {
    storage_access().tick(millis());
  }
#endif
  delay(1);
}
