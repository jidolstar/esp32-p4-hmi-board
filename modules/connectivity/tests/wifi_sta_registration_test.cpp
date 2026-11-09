#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "connectivity/virtual_wifi_sta.hpp"
#include "hmi/connectivity/wifi_sta_registration.hpp"
#include "hmi/connectivity/wifi_station_settings.hpp"

namespace {

using hmi::connectivity::CredentialInputSource;
using hmi::connectivity::CredentialValidationError;
using hmi::connectivity::WifiRegistrationError;
using hmi::connectivity::WifiRegistrationSnapshot;
using hmi::connectivity::WifiRegistrationState;
using hmi::connectivity::WifiScanState;
using hmi::connectivity::WifiSecurity;
using hmi::connectivity::WifiStaRegistration;
using hmi::connectivity::WifiStationCredential;
using hmi::platform::connectivity::MemoryWifiStationSettingsStore;
using hmi::platform::connectivity::VirtualWifiScenario;
using hmi::platform::connectivity::VirtualWifiSta;

constexpr std::uint8_t kPassword[] = "starter123";

void advance(WifiStaRegistration& registration, std::uint32_t& now_ms,
             const unsigned ticks, const std::uint32_t step_ms = 1U) {
  for (unsigned tick = 0U; tick < ticks; ++tick) {
    now_ms += step_ms;
    registration.tick(now_ms);
  }
}

bool contains_bytes(const void* memory, const std::size_t memory_size,
                    const std::uint8_t* pattern,
                    const std::size_t pattern_size) {
  if (pattern_size == 0U || pattern_size > memory_size) {
    return false;
  }
  const auto* bytes = static_cast<const std::uint8_t*>(memory);
  for (std::size_t index = 0U; index + pattern_size <= memory_size; ++index) {
    if (std::memcmp(bytes + index, pattern, pattern_size) == 0) {
      return true;
    }
  }
  return false;
}

void verify_credential_validation() {
  WifiStationCredential credential{};
  assert(hmi::connectivity::make_wifi_station_credential(
             nullptr, 0U, kPassword, sizeof(kPassword) - 1U,
             CredentialInputSource::hmi_touch, credential) ==
         CredentialValidationError::empty_ssid);

  std::uint8_t long_ssid[33]{};
  std::memset(long_ssid, 'A', sizeof(long_ssid));
  assert(hmi::connectivity::make_wifi_station_credential(
             long_ssid, sizeof(long_ssid), kPassword,
             sizeof(kPassword) - 1U, CredentialInputSource::hmi_touch,
             credential) == CredentialValidationError::ssid_too_long);

  const std::uint8_t utf8_ssid[] = {0xEDU, 0x95U, 0x9CU};
  assert(hmi::connectivity::make_wifi_station_credential(
             utf8_ssid, sizeof(utf8_ssid), kPassword,
             sizeof(kPassword) - 1U, CredentialInputSource::hmi_touch,
             credential) ==
         CredentialValidationError::ssid_not_printable_ascii);
  assert(hmi::connectivity::make_wifi_station_credential(
             utf8_ssid, sizeof(utf8_ssid), kPassword,
             sizeof(kPassword) - 1U, CredentialInputSource::service_tool,
             credential) == CredentialValidationError::none);
  hmi::connectivity::clear_wifi_station_credential(credential);

  std::uint8_t psk[64]{};
  std::memset(psk, 'a', sizeof(psk));
  const auto* ssid = reinterpret_cast<const std::uint8_t*>("HMI-LAB");
  assert(hmi::connectivity::make_wifi_station_credential(
             ssid, 7U, psk, sizeof(psk), CredentialInputSource::hmi_touch,
             credential) == CredentialValidationError::none);
  psk[0] = 'g';
  assert(hmi::connectivity::make_wifi_station_credential(
             ssid, 7U, psk, sizeof(psk), CredentialInputSource::hmi_touch,
             credential) == CredentialValidationError::password_64_not_hex);
  hmi::connectivity::clear_wifi_station_credential(credential);
}

void verify_normal_scan_connect_and_secret_boundary() {
  VirtualWifiSta port;
  MemoryWifiStationSettingsStore store;
  WifiStaRegistration registration(port, store);
  std::uint32_t now_ms = 0U;
  assert(registration.begin(now_ms, true));
  assert(registration.request_scan());
  advance(registration, now_ms, 5U);

  const auto scan = registration.scan_snapshot();
  assert(scan.state == WifiScanState::ready);
  assert(scan.count == 4U);  // hidden SSID 제거 + 같은 SSID/security 중복 제거.
  assert(registration.scan_entry(scan.generation, 0U) != nullptr);
  assert(std::strcmp(registration.scan_entry(scan.generation, 0U)->ssid,
                     "HMI-LAB") == 0);
  assert(!registration.connect_selected(scan.generation - 1U, 0U, kPassword,
                                        sizeof(kPassword) - 1U));
  assert(registration.snapshot().error ==
         WifiRegistrationError::stale_selection);
  assert(registration.connect_selected(scan.generation, 0U, kPassword,
                                       sizeof(kPassword) - 1U));
  advance(registration, now_ms, 5U);
  const WifiRegistrationSnapshot snapshot = registration.snapshot();
  assert(snapshot.state == WifiRegistrationState::saved);
  assert(snapshot.has_ip && snapshot.link_connected);
  assert(std::strcmp(snapshot.ipv4, "192.168.40.42") == 0);
  assert(store.save_count() == 1U);
  assert(!contains_bytes(&snapshot, sizeof(snapshot), kPassword,
                         sizeof(kPassword) - 1U));
}

void verify_empty_scan_and_late_cancelled_result() {
  VirtualWifiSta port;
  port.set_scenario(VirtualWifiScenario::empty_scan);
  MemoryWifiStationSettingsStore store;
  WifiStaRegistration registration(port, store);
  std::uint32_t now_ms = 0U;
  assert(registration.begin(now_ms, false));
  assert(registration.request_scan());
  advance(registration, now_ms, 5U);
  assert(registration.scan_snapshot().state == WifiScanState::ready);
  assert(registration.scan_snapshot().count == 0U);

  port.set_scenario(VirtualWifiScenario::normal);
  assert(registration.request_scan());
  advance(registration, now_ms, 1U);
  const std::uint32_t cancelled_generation =
      registration.scan_snapshot().generation;
  registration.cancel();
  assert(registration.scan_snapshot().draining);
  advance(registration, now_ms, 5U);
  assert(!registration.scan_snapshot().draining);
  assert(registration.scan_snapshot().generation != cancelled_generation);
  assert(registration.scan_snapshot().count == 0U);
  assert(registration.request_scan());
}

void verify_failure_timeout_and_disconnect() {
  {
    VirtualWifiSta port;
    port.set_scenario(VirtualWifiScenario::authentication_failure);
    MemoryWifiStationSettingsStore store;
    WifiStaRegistration registration(port, store);
    std::uint32_t now_ms = 0U;
    assert(registration.begin(now_ms, false));
    const auto* ssid = reinterpret_cast<const std::uint8_t*>("HMI-LAB");
    assert(registration.connect_hidden(ssid, 7U, kPassword,
                                       sizeof(kPassword) - 1U,
                                       WifiSecurity::personal));
    advance(registration, now_ms, 5U);
    assert(registration.snapshot().state == WifiRegistrationState::failed);
    assert(registration.snapshot().error ==
           WifiRegistrationError::authentication_failed);
    assert(store.save_count() == 0U);
  }

  {
    VirtualWifiSta port;
    port.set_scenario(VirtualWifiScenario::connection_timeout);
    MemoryWifiStationSettingsStore store;
    WifiStaRegistration registration(port, store);
    std::uint32_t now_ms = 0U;
    assert(registration.begin(now_ms, false));
    const auto* ssid =
        reinterpret_cast<const std::uint8_t*>("Guest-Network");
    assert(registration.connect_hidden(ssid, 13U, nullptr, 0U,
                                       WifiSecurity::open));
    advance(registration, now_ms, 1U, 20001U);
    assert(registration.snapshot().state == WifiRegistrationState::failed);
    assert(registration.snapshot().error == WifiRegistrationError::timeout);
  }

  {
    VirtualWifiSta port;
    MemoryWifiStationSettingsStore store;
    WifiStaRegistration registration(port, store);
    std::uint32_t now_ms = 0U;
    assert(registration.begin(now_ms, false));
    const auto* ssid =
        reinterpret_cast<const std::uint8_t*>("Guest-Network");
    assert(registration.connect_hidden(ssid, 13U, nullptr, 0U,
                                       WifiSecurity::open));
    advance(registration, now_ms, 5U);
    assert(registration.snapshot().state == WifiRegistrationState::saved);
    port.force_disconnect();
    advance(registration, now_ms, 1U);
    assert(registration.snapshot().state == WifiRegistrationState::failed);
    assert(!registration.snapshot().has_ip);
  }
}

void verify_store_failure_and_restore() {
  VirtualWifiSta port;
  MemoryWifiStationSettingsStore store;
  store.set_fail_save(true);
  WifiStaRegistration registration(port, store);
  std::uint32_t now_ms = 0U;
  assert(registration.begin(now_ms, false));
  const auto* ssid = reinterpret_cast<const std::uint8_t*>("Guest-Network");
  assert(registration.connect_hidden(ssid, 13U, nullptr, 0U,
                                     WifiSecurity::open));
  advance(registration, now_ms, 5U);
  assert(registration.snapshot().state ==
         WifiRegistrationState::connected_not_saved);
  assert(registration.snapshot().error == WifiRegistrationError::storage_save);
  store.set_fail_save(false);
  assert(registration.retry_save());
  assert(registration.snapshot().state == WifiRegistrationState::saved);

  VirtualWifiSta reboot_port;
  WifiStaRegistration reboot(reboot_port, store);
  assert(reboot.begin(now_ms, true));
  advance(reboot, now_ms, 5U);
  assert(reboot.snapshot().state == WifiRegistrationState::saved);
}

}  // namespace

int main() {
  verify_credential_validation();
  verify_normal_scan_connect_and_secret_boundary();
  verify_empty_scan_and_late_cancelled_result();
  verify_failure_timeout_and_disconnect();
  verify_store_failure_and_restore();
  return 0;
}
