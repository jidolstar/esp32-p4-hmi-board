#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "connectivity/virtual_wifi_ap.hpp"
#include "connectivity/virtual_wifi_sta.hpp"
#include "hmi/connectivity/wifi_ap_runtime.hpp"

namespace {

using hmi::connectivity::WifiApRuntime;
using hmi::connectivity::WifiApRuntimeError;
using hmi::connectivity::WifiApRuntimeState;
using hmi::connectivity::WifiApStationPolicy;
using hmi::connectivity::WifiRegistrationState;
using hmi::connectivity::WifiSecurity;
using hmi::connectivity::WifiStaRegistration;
using hmi::platform::connectivity::MemoryWifiStationSettingsStore;
using hmi::platform::connectivity::VirtualWifiAp;
using hmi::platform::connectivity::VirtualWifiApScenario;
using hmi::platform::connectivity::VirtualWifiSta;

void step(WifiStaRegistration& station, WifiApRuntime& access_point,
          std::uint32_t& now_ms, const unsigned count = 1U) {
  for (unsigned index = 0U; index < count; ++index) {
    now_ms += 100U;
    station.tick(now_ms);
    access_point.tick(now_ms);
  }
}

void test_start_stop_restart() {
  VirtualWifiSta station_port;
  MemoryWifiStationSettingsStore store;
  WifiStaRegistration station(station_port, store);
  assert(station.begin(0U, true));
  VirtualWifiAp ap_port;
  WifiApRuntime access_point(station, ap_port);
  std::uint32_t now_ms = 0U;

  assert(access_point.start("HMI-Control-A0", "hmi-a0-test", now_ms));
  assert(!access_point.start("duplicate", "duplicate", now_ms));
  step(station, access_point, now_ms, 3U);
  assert(access_point.snapshot().state == WifiApRuntimeState::ready);
  assert(access_point.snapshot().health_server_ready);
  assert(station.suspended_for_ap());

  ap_port.set_client_count(1U);
  step(station, access_point, now_ms);
  assert(access_point.snapshot().client_count == 1U);
  assert(access_point.stop(now_ms));
  assert(!access_point.stop(now_ms));
  step(station, access_point, now_ms, 4U);
  assert(access_point.snapshot().state == WifiApRuntimeState::off);
  assert(!station.suspended_for_ap());

  assert(access_point.start("HMI-Control-A0", "hmi-a0-test", now_ms));
  step(station, access_point, now_ms, 3U);
  assert(access_point.snapshot().state == WifiApRuntimeState::ready);
  assert(access_point.stop(now_ms));
  step(station, access_point, now_ms, 4U);
  assert(access_point.snapshot().state == WifiApRuntimeState::off);
}

void test_saved_sta_restore() {
  VirtualWifiSta station_port;
  MemoryWifiStationSettingsStore store;
  WifiStaRegistration station(station_port, store);
  assert(station.begin(0U, true));
  constexpr char ssid[] = "HMI-LAB";
  constexpr char password[] = "starter123";
  assert(station.connect_service(
      reinterpret_cast<const std::uint8_t*>(ssid), sizeof(ssid) - 1U,
      reinterpret_cast<const std::uint8_t*>(password), sizeof(password) - 1U,
      WifiSecurity::personal));
  std::uint32_t now_ms = 0U;
  for (unsigned index = 0U; index < 6U; ++index) {
    now_ms += 100U;
    station.tick(now_ms);
  }
  assert(station.snapshot().state == WifiRegistrationState::saved);
  assert(station.snapshot().has_ip);

  VirtualWifiAp ap_port;
  WifiApRuntime access_point(station, ap_port);
  assert(access_point.start("HMI-Control-A0", "hmi-a0-test", now_ms));
  step(station, access_point, now_ms, 3U);
  assert(access_point.snapshot().state == WifiApRuntimeState::ready);
  assert(access_point.stop(now_ms));
  step(station, access_point, now_ms, 10U);
  assert(access_point.snapshot().state == WifiApRuntimeState::off);
  assert(station.snapshot().state == WifiRegistrationState::saved);
  assert(station.snapshot().has_ip);
}

void test_saved_sta_coexists_without_reconnect() {
  VirtualWifiSta station_port;
  MemoryWifiStationSettingsStore store;
  WifiStaRegistration station(station_port, store);
  assert(station.begin(0U, true));
  constexpr char ssid[] = "HMI-LAB";
  constexpr char password[] = "starter123";
  assert(station.connect_service(
      reinterpret_cast<const std::uint8_t*>(ssid), sizeof(ssid) - 1U,
      reinterpret_cast<const std::uint8_t*>(password), sizeof(password) - 1U,
      WifiSecurity::personal));
  std::uint32_t now_ms = 0U;
  for (unsigned index = 0U; index < 6U; ++index) {
    now_ms += 100U;
    station.tick(now_ms);
  }
  assert(station.snapshot().state == WifiRegistrationState::saved);
  assert(station.snapshot().has_ip);

  VirtualWifiAp ap_port;
  WifiApRuntime access_point(station, ap_port,
                             WifiApStationPolicy::keep_connected);
  assert(access_point.start("HMI-Control-A0", "hmi-a0-test", now_ms));
  step(station, access_point, now_ms, 3U);
  assert(access_point.snapshot().state == WifiApRuntimeState::ready);
  assert(access_point.snapshot().sta_has_ip);
  assert(!station.suspended_for_ap());
  assert(station.snapshot().state == WifiRegistrationState::saved);
  assert(station.snapshot().has_ip);
  char sta_ipv4[16]{};
  access_point.copy_sta_ipv4(sta_ipv4, sizeof(sta_ipv4));
  assert(std::strcmp(sta_ipv4, "192.168.40.42") == 0);

  assert(access_point.stop(now_ms));
  step(station, access_point, now_ms, 3U);
  assert(access_point.snapshot().state == WifiApRuntimeState::off);
  assert(access_point.snapshot().sta_has_ip);
  assert(station.snapshot().state == WifiRegistrationState::saved);
  assert(station.snapshot().has_ip);
}

void test_failure_and_timeout() {
  {
    VirtualWifiSta station_port;
    MemoryWifiStationSettingsStore store;
    WifiStaRegistration station(station_port, store);
    assert(station.begin(0U, true));
    VirtualWifiAp ap_port;
    ap_port.set_scenario(VirtualWifiApScenario::start_failure);
    WifiApRuntime access_point(station, ap_port);
    assert(access_point.start("HMI-Control-A0", "hmi-a0-test", 0U));
    station.tick(100U);
    access_point.tick(100U);
    assert(access_point.snapshot().state == WifiApRuntimeState::failed);
    assert(access_point.snapshot().error == WifiApRuntimeError::start_failed);
    assert(!station.suspended_for_ap());
  }
  {
    VirtualWifiSta station_port;
    MemoryWifiStationSettingsStore store;
    WifiStaRegistration station(station_port, store);
    assert(station.begin(0U, true));
    VirtualWifiAp ap_port;
    ap_port.set_scenario(VirtualWifiApScenario::start_timeout);
    WifiApRuntime access_point(station, ap_port);
    assert(access_point.start("HMI-Control-A0", "hmi-a0-test", 0U));
    station.tick(100U);
    access_point.tick(100U);
    station.tick(5200U);
    access_point.tick(5200U);
    assert(access_point.snapshot().state == WifiApRuntimeState::failed);
    assert(access_point.snapshot().error == WifiApRuntimeError::start_timeout);
    assert(!station.suspended_for_ap());
  }
}

}  // namespace

int main() {
  std::fprintf(stderr, "wifi_ap_runtime: start-stop-restart\n");
  test_start_stop_restart();
  std::fprintf(stderr, "wifi_ap_runtime: sta-restore\n");
  test_saved_sta_restore();
  std::fprintf(stderr, "wifi_ap_runtime: sta-coexist\n");
  test_saved_sta_coexists_without_reconnect();
  std::fprintf(stderr, "wifi_ap_runtime: failure-timeout\n");
  test_failure_and_timeout();
  std::fprintf(stderr, "wifi_ap_runtime: ok\n");
  return 0;
}
