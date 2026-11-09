#pragma once

#include <memory>

#include "hmi/connectivity/wifi_sta_port.hpp"

namespace hmi::platform::connectivity {

/**
 * ESP32-P4의 ESP-Hosted RF를 공통 STA 계약으로 옮긴다.
 *
 * 구현은 ESP-Hosted handle, FreeRTOS worker와 callback을 숨긴다. worker는 UI 객체를
 * 참조하지 않으며 scan batch 또는 RSSI sample만 완료 시점까지 소유한다.
 */
class EspHostedWifiSta final : public hmi::connectivity::WifiStaPort {
 public:
  EspHostedWifiSta();
  ~EspHostedWifiSta() override;

  EspHostedWifiSta(const EspHostedWifiSta&) = delete;
  EspHostedWifiSta& operator=(const EspHostedWifiSta&) = delete;

  bool begin_scan(std::uint32_t generation) override;
  [[nodiscard]] bool scan_running() const override;
  hmi::connectivity::WifiScanBatch take_scan_result() override;
  void cancel_scan() override;
  [[nodiscard]] unsigned scan_attempt() const override;
  void poll() override;
  bool connect(
      const hmi::connectivity::WifiStationCredential& credential) override;
  void disconnect() override;
  [[nodiscard]] hmi::connectivity::WifiLinkState link_state() const override;
  [[nodiscard]] bool has_ip() const override;
  void copy_ipv4(char* output, std::size_t capacity) const override;
  bool begin_signal_sample(std::uint32_t generation) override;
  [[nodiscard]] bool signal_sample_running() const override;
  hmi::connectivity::WifiSignalSample take_signal_sample() override;
  [[nodiscard]] bool ready() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_{};
};

}  // namespace hmi::platform::connectivity
