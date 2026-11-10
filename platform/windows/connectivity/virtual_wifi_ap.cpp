#include "connectivity/virtual_wifi_ap.hpp"

#include <cstring>

namespace hmi::platform::connectivity {

bool VirtualWifiAp::start(const hmi::connectivity::WifiApConfig& config) {
  if (state_ != hmi::connectivity::WifiApPortState::off ||
      config.ssid_length == 0U || config.password_length < 8U) {
    return false;
  }
  if (scenario_ == VirtualWifiApScenario::start_failure) {
    state_ = hmi::connectivity::WifiApPortState::failed;
    return false;
  }
  state_ = hmi::connectivity::WifiApPortState::starting;
  transition_ticks_ = 2U;
  return true;
}

void VirtualWifiAp::request_stop() {
  if (state_ == hmi::connectivity::WifiApPortState::off) {
    return;
  }
  state_ = hmi::connectivity::WifiApPortState::stopping;
  transition_ticks_ = 2U;
  client_count_ = 0U;
}

void VirtualWifiAp::poll() {
  if (transition_ticks_ == 0U) {
    return;
  }
  if (state_ == hmi::connectivity::WifiApPortState::starting &&
      scenario_ == VirtualWifiApScenario::start_timeout) {
    return;
  }
  if (state_ == hmi::connectivity::WifiApPortState::stopping &&
      scenario_ == VirtualWifiApScenario::stop_timeout) {
    return;
  }
  --transition_ticks_;
  if (transition_ticks_ != 0U) {
    return;
  }
  state_ = state_ == hmi::connectivity::WifiApPortState::starting
               ? hmi::connectivity::WifiApPortState::ready
               : hmi::connectivity::WifiApPortState::off;
}

hmi::connectivity::WifiApPortSnapshot VirtualWifiAp::snapshot()
    const noexcept {
  return {state_, client_count_,
          state_ == hmi::connectivity::WifiApPortState::ready};
}

void VirtualWifiAp::copy_ipv4(char* output,
                              const std::size_t capacity) const noexcept {
  if (output == nullptr || capacity == 0U) {
    return;
  }
  const char* address =
      state_ == hmi::connectivity::WifiApPortState::ready ? "192.168.4.1" : "";
  std::strncpy(output, address, capacity - 1U);
  output[capacity - 1U] = '\0';
}

}  // namespace hmi::platform::connectivity
