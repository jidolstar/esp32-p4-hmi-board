#include "connectivity/virtual_wifi_sta.hpp"

#include <cstring>
#include <new>

#include "hmi/connectivity/wifi_station_settings.hpp"

namespace hmi::platform::connectivity {
namespace {

struct VirtualAccessPoint {
  const char* ssid;
  hmi::connectivity::WifiSecurity security;
  std::int16_t rssi;
  const char* password;
};

constexpr VirtualAccessPoint kAccessPoints[] = {
    {"HMI-LAB", hmi::connectivity::WifiSecurity::personal, -38,
     "starter123"},
    {"Guest-Network", hmi::connectivity::WifiSecurity::open, -52, ""},
    {"Office-WPA3", hmi::connectivity::WifiSecurity::wpa3_personal, -65,
     "starter123"},
    {"Legacy-Enterprise", hmi::connectivity::WifiSecurity::unsupported, -74,
     ""},
    {"HMI-LAB", hmi::connectivity::WifiSecurity::personal, -82,
     "starter123"},
    {"", hmi::connectivity::WifiSecurity::personal, -90, "starter123"},
};

const VirtualAccessPoint* find_access_point(const char* ssid) noexcept {
  for (const VirtualAccessPoint& access_point : kAccessPoints) {
    if (std::strcmp(access_point.ssid, ssid) == 0) {
      return &access_point;
    }
  }
  return nullptr;
}

}  // namespace

void VirtualWifiSta::set_scenario(
    const VirtualWifiScenario scenario) noexcept {
  scenario_ = scenario;
}

void VirtualWifiSta::force_disconnect() noexcept {
  link_ = hmi::connectivity::WifiLinkState::failed;
  ip_ = false;
  connect_ticks_ = 0U;
}

bool VirtualWifiSta::begin_scan(const std::uint32_t generation) {
  if (scan_running() ||
      link_ == hmi::connectivity::WifiLinkState::connecting) {
    return false;
  }
  scan_generation_ = generation;
  scan_ticks_ = 3U;
  scan_cancelled_ = false;
  return true;
}

bool VirtualWifiSta::scan_running() const { return scan_ticks_ != 0U; }

hmi::connectivity::WifiScanBatch VirtualWifiSta::take_scan_result() {
  hmi::connectivity::WifiScanBatch result{};
  result.generation = scan_generation_;
  if (scan_running()) {
    return result;
  }
  if (scan_cancelled_) {
    // service가 generation/state로 늦은 완료를 버리는 경로를 재현한다.
    result.succeeded = true;
    return result;
  }
  if (scenario_ == VirtualWifiScenario::scan_failure) {
    return result;
  }
  if (scenario_ == VirtualWifiScenario::empty_scan) {
    result.succeeded = true;
    return result;
  }

  constexpr std::size_t count = sizeof(kAccessPoints) / sizeof(kAccessPoints[0]);
  result.entries.reset(
      new (std::nothrow) hmi::connectivity::WifiScanEntry[count]{});
  if (!result.entries) {
    return result;
  }
  result.count = static_cast<std::uint16_t>(count);
  result.succeeded = true;
  for (std::size_t index = 0U; index < count; ++index) {
    auto& entry = result.entries[index];
    entry.ssid_length = static_cast<std::uint8_t>(
        std::strlen(kAccessPoints[index].ssid));
    std::memcpy(entry.ssid, kAccessPoints[index].ssid, entry.ssid_length);
    entry.security = kAccessPoints[index].security;
    entry.rssi = kAccessPoints[index].rssi;
    entry.channel = static_cast<std::uint8_t>(index + 1U);
    entry.bssid[5] = static_cast<std::uint8_t>(index + 1U);
  }
  return result;
}

void VirtualWifiSta::cancel_scan() { scan_cancelled_ = true; }

void VirtualWifiSta::poll() {
  if (scan_ticks_ != 0U) {
    --scan_ticks_;
  }
  if (connect_ticks_ != 0U &&
      scenario_ != VirtualWifiScenario::connection_timeout) {
    --connect_ticks_;
    if (connect_ticks_ == 0U) {
      link_ = connect_outcome_;
      ip_ = link_ == hmi::connectivity::WifiLinkState::connected;
    }
  }
  if (link_ == hmi::connectivity::WifiLinkState::connected) {
    ++connected_ticks_;
    if (scenario_ == VirtualWifiScenario::disconnect_after_connect &&
        connected_ticks_ >= 3U) {
      force_disconnect();
    }
  }
}

bool VirtualWifiSta::connect(
    const hmi::connectivity::WifiStationCredential& credential) {
  const VirtualAccessPoint* access_point = find_access_point(credential.ssid);
  if (access_point == nullptr || access_point->ssid[0] == '\0') {
    connect_outcome_ = hmi::connectivity::WifiLinkState::no_access_point;
  } else if (scenario_ == VirtualWifiScenario::authentication_failure ||
             (access_point->security != hmi::connectivity::WifiSecurity::open &&
              std::strcmp(access_point->password, credential.password) != 0)) {
    connect_outcome_ =
        hmi::connectivity::WifiLinkState::authentication_failed;
  } else {
    connect_outcome_ = hmi::connectivity::WifiLinkState::connected;
  }
  link_ = hmi::connectivity::WifiLinkState::connecting;
  connect_ticks_ = 4U;
  connected_ticks_ = 0U;
  ip_ = false;
  connected_rssi_ = access_point == nullptr ? 0 : access_point->rssi;
  return true;
}

void VirtualWifiSta::disconnect() {
  link_ = hmi::connectivity::WifiLinkState::idle;
  connect_ticks_ = 0U;
  connected_ticks_ = 0U;
  ip_ = false;
  connected_rssi_ = 0;
}

bool VirtualWifiSta::has_ip() const {
  return link_ == hmi::connectivity::WifiLinkState::connected && ip_;
}

void VirtualWifiSta::copy_ipv4(char* output,
                               const std::size_t capacity) const {
  if (output == nullptr || capacity == 0U) {
    return;
  }
  std::strncpy(output, has_ip() ? "192.168.40.42" : "", capacity - 1U);
  output[capacity - 1U] = '\0';
}

bool VirtualWifiSta::begin_signal_sample(const std::uint32_t generation) {
  if (!has_ip()) {
    return false;
  }
  signal_sample_ = {generation, connected_rssi_, connected_rssi_ != 0};
  return true;
}

hmi::connectivity::WifiSignalSample
VirtualWifiSta::take_signal_sample() {
  const hmi::connectivity::WifiSignalSample sample = signal_sample_;
  signal_sample_ = {};
  return sample;
}

MemoryWifiStationSettingsStore::~MemoryWifiStationSettingsStore() {
  hmi::connectivity::clear_wifi_station_credential(saved_);
}

hmi::connectivity::WifiStationSettingsLoadStatus
MemoryWifiStationSettingsStore::load(
    hmi::connectivity::WifiStationCredential& output) {
  if (fail_load_) {
    return hmi::connectivity::WifiStationSettingsLoadStatus::storage_error;
  }
  if (!present_) {
    return hmi::connectivity::WifiStationSettingsLoadStatus::not_found;
  }
  output = saved_;
  return hmi::connectivity::WifiStationSettingsLoadStatus::loaded;
}

bool MemoryWifiStationSettingsStore::save(
    const hmi::connectivity::WifiStationCredential& credential) {
  ++save_count_;
  if (fail_save_) {
    return false;
  }
  saved_ = credential;
  present_ = true;
  return true;
}

bool MemoryWifiStationSettingsStore::erase() {
  if (fail_erase_) {
    return false;
  }
  hmi::connectivity::clear_wifi_station_credential(saved_);
  present_ = false;
  return true;
}

}  // namespace hmi::platform::connectivity
