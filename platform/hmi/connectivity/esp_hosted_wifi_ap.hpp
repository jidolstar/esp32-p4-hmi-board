#pragma once

#include <memory>

#include "hmi/connectivity/wifi_ap_port.hpp"

namespace hmi::connectivity {
class WifiApWebController;
}

namespace hmi::platform::connectivity {

/** ESP32-P4의 ESP-Hosted SoftAP와 P4 HTTP health endpoint adapter다. */
class EspHostedWifiAp final : public hmi::connectivity::WifiApPort {
 public:
  EspHostedWifiAp();
  ~EspHostedWifiAp() override;

  EspHostedWifiAp(const EspHostedWifiAp&) = delete;
  EspHostedWifiAp& operator=(const EspHostedWifiAp&) = delete;

  bool start(const hmi::connectivity::WifiApConfig& config) override;
  void request_stop() override;
  void poll() override;
  [[nodiscard]] hmi::connectivity::WifiApPortSnapshot snapshot()
      const noexcept override;
  void copy_ipv4(char* output, std::size_t capacity) const noexcept override;
  /** AP를 시작하기 전에 page-independent web controller를 연결한다. */
  void attach_web_controller(
      hmi::connectivity::WifiApWebController* controller) noexcept;
  [[nodiscard]] bool ready() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_{};
};

static_assert(sizeof(EspHostedWifiAp) <= 16U,
              "AP adapter exposes only one bounded pimpl handle");

}  // namespace hmi::platform::connectivity
