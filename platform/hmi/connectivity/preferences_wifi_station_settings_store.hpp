#pragma once

#include "hmi/connectivity/wifi_station_settings_store.hpp"

namespace hmi::platform::connectivity {

/**
 * profile NVS namespace 안에 checksum 포함 credential record 하나를 저장한다.
 *
 * namespace 문자열은 이 객체보다 오래 살아야 한다. password는 log하지 않으며 load/save
 * 임시 record를 반환 전에 명시적으로 지운다.
 */
class PreferencesWifiStationSettingsStore final
    : public hmi::connectivity::WifiStationSettingsStore {
 public:
  explicit PreferencesWifiStationSettingsStore(
      const char* nvs_namespace) noexcept;

  hmi::connectivity::WifiStationSettingsLoadStatus load(
      hmi::connectivity::WifiStationCredential& output) override;
  bool save(
      const hmi::connectivity::WifiStationCredential& credential) override;
  bool erase() override;

 private:
  const char* nvs_namespace_ = nullptr;
  bool valid_ = false;
};

}  // namespace hmi::platform::connectivity
