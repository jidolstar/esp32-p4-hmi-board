#include "connectivity/preferences_wifi_station_settings_store.hpp"

#include <Preferences.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "hmi/connectivity/wifi_station_settings.hpp"
#include "log.hpp"

namespace hmi::platform::connectivity {
namespace {

constexpr char kCredentialKey[] = "wifi_cred";
constexpr std::uint32_t kRecordMagic = 0x41545357U;  // "WSTA"
constexpr std::uint16_t kRecordSchema = 1U;

/** padding까지 zero-init한 뒤 checksum하는 단일 NVS blob이다. */
struct WifiStationSettingsRecord {
  std::uint32_t magic = kRecordMagic;
  std::uint16_t schema = kRecordSchema;
  std::uint16_t reserved = 0U;
  hmi::connectivity::WifiStationCredential credential{};
  std::uint32_t checksum = 0U;
};

static_assert(sizeof(WifiStationSettingsRecord) <= 128U,
              "Wi-Fi NVS record must remain bounded");

void clear_record(WifiStationSettingsRecord& record) noexcept {
  auto* bytes = reinterpret_cast<volatile unsigned char*>(&record);
  for (std::size_t index = 0U; index < sizeof(record); ++index) {
    bytes[index] = 0U;
  }
}

std::uint32_t record_checksum(
    const WifiStationSettingsRecord& record) noexcept {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&record);
  constexpr std::size_t checked_size =
      offsetof(WifiStationSettingsRecord, checksum);
  std::uint32_t hash = 2166136261U;
  for (std::size_t index = 0U; index < checked_size; ++index) {
    hash = (hash ^ bytes[index]) * 16777619U;
  }
  return hash;
}

bool valid_record(const WifiStationSettingsRecord& record) noexcept {
  return record.magic == kRecordMagic && record.schema == kRecordSchema &&
         record.checksum == record_checksum(record) &&
         hmi::connectivity::validate_wifi_station_credential(
             record.credential,
             hmi::connectivity::CredentialInputSource::service_tool) ==
             hmi::connectivity::CredentialValidationError::none;
}

}  // namespace

PreferencesWifiStationSettingsStore::PreferencesWifiStationSettingsStore(
    const char* nvs_namespace) noexcept
    : nvs_namespace_(nvs_namespace),
      valid_(nvs_namespace != nullptr && nvs_namespace[0] != '\0' &&
             std::strlen(nvs_namespace) <= 15U) {}

hmi::connectivity::WifiStationSettingsLoadStatus
PreferencesWifiStationSettingsStore::load(
    hmi::connectivity::WifiStationCredential& output) {
  if (!valid_) {
    return hmi::connectivity::WifiStationSettingsLoadStatus::storage_error;
  }
  Preferences preferences;
  if (!preferences.begin(nvs_namespace_, false)) {
    HMI_LOG_ERROR("[connectivity] credential NVS open failed\r\n");
    return hmi::connectivity::WifiStationSettingsLoadStatus::storage_error;
  }
  const std::size_t stored_size = preferences.getBytesLength(kCredentialKey);
  if (stored_size == 0U) {
    preferences.end();
    return hmi::connectivity::WifiStationSettingsLoadStatus::not_found;
  }
  if (stored_size != sizeof(WifiStationSettingsRecord)) {
    preferences.end();
    HMI_LOG_ERROR("[connectivity] credential record size invalid bytes=%u\r\n",
                  static_cast<unsigned>(stored_size));
    return hmi::connectivity::WifiStationSettingsLoadStatus::invalid;
  }

  WifiStationSettingsRecord record{};
  const bool read = preferences.getBytes(kCredentialKey, &record,
                                         sizeof(record)) == sizeof(record);
  preferences.end();
  if (!read) {
    clear_record(record);
    return hmi::connectivity::WifiStationSettingsLoadStatus::storage_error;
  }
  if (!valid_record(record)) {
    clear_record(record);
    HMI_LOG_ERROR("[connectivity] credential record validation failed\r\n");
    return hmi::connectivity::WifiStationSettingsLoadStatus::invalid;
  }
  output = record.credential;
  clear_record(record);
  return hmi::connectivity::WifiStationSettingsLoadStatus::loaded;
}

bool PreferencesWifiStationSettingsStore::save(
    const hmi::connectivity::WifiStationCredential& credential) {
  if (!valid_ ||
      hmi::connectivity::validate_wifi_station_credential(
          credential,
          hmi::connectivity::CredentialInputSource::service_tool) !=
          hmi::connectivity::CredentialValidationError::none) {
    return false;
  }

  WifiStationSettingsRecord record{};
  record.credential = credential;
  record.checksum = record_checksum(record);
  Preferences preferences;
  if (!preferences.begin(nvs_namespace_, false)) {
    clear_record(record);
    HMI_LOG_ERROR("[connectivity] credential NVS open failed while saving\r\n");
    return false;
  }
  const bool saved =
      preferences.putBytes(kCredentialKey, &record, sizeof(record)) ==
      sizeof(record);
  WifiStationSettingsRecord readback{};
  const bool verified =
      saved &&
      preferences.getBytes(kCredentialKey, &readback, sizeof(readback)) ==
          sizeof(readback) &&
      valid_record(readback) &&
      std::memcmp(&readback, &record, sizeof(record)) == 0;
  preferences.end();
  clear_record(record);
  clear_record(readback);
  if (!verified) {
    HMI_LOG_ERROR("[connectivity] credential NVS write/readback failed\r\n");
  }
  return verified;
}

bool PreferencesWifiStationSettingsStore::erase() {
  if (!valid_) {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(nvs_namespace_, false)) {
    HMI_LOG_ERROR("[connectivity] credential NVS open failed while erasing\r\n");
    return false;
  }
  const bool erased =
      !preferences.isKey(kCredentialKey) || preferences.remove(kCredentialKey);
  preferences.end();
  if (!erased) {
    HMI_LOG_ERROR("[connectivity] credential NVS erase failed\r\n");
  }
  return erased;
}

}  // namespace hmi::platform::connectivity
