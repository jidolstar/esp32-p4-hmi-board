#pragma once

#include <cstdint>

#include "hmi/connectivity/wifi_ap_port.hpp"

namespace hmi::platform::connectivity {

enum class VirtualWifiApScenario : std::uint8_t {
  normal,
  start_failure,
  start_timeout,
  stop_timeout,
};

/** PC 네트워크를 변경하지 않고 AP 수명과 health endpoint 상태를 재현한다. */
class VirtualWifiAp final : public hmi::connectivity::WifiApPort {
 public:
  void set_scenario(VirtualWifiApScenario scenario) noexcept {
    scenario_ = scenario;
  }
  void set_client_count(std::uint8_t count) noexcept { client_count_ = count; }

  bool start(const hmi::connectivity::WifiApConfig& config) override;
  void request_stop() override;
  void poll() override;
  [[nodiscard]] hmi::connectivity::WifiApPortSnapshot snapshot()
      const noexcept override;
  void copy_ipv4(char* output, std::size_t capacity) const noexcept override;

 private:
  VirtualWifiApScenario scenario_ = VirtualWifiApScenario::normal;
  hmi::connectivity::WifiApPortState state_ =
      hmi::connectivity::WifiApPortState::off;
  std::uint8_t transition_ticks_ = 0U;
  std::uint8_t client_count_ = 0U;
};

static_assert(sizeof(VirtualWifiAp) <= 16U,
              "Virtual AP keeps only deterministic scalar state");

}  // namespace hmi::platform::connectivity
