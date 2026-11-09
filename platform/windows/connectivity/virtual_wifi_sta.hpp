#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/wifi_sta_port.hpp"
#include "hmi/connectivity/wifi_station_settings_store.hpp"

namespace hmi::platform::connectivity {

enum class VirtualWifiScenario : std::uint8_t {
  normal,
  empty_scan,
  scan_failure,
  authentication_failure,
  connection_timeout,
  disconnect_after_connect,
};

/**
 * sample-suite Windows용 결정적 AP/IP fixture다.
 *
 * 실제 PC 네트워크를 읽거나 변경하지 않는다. scenario와 poll 횟수만으로 검색, 연결,
 * timeout과 끊김을 재현하며 전달받은 password pointer를 호출 뒤 보관하지 않는다.
 */
class VirtualWifiSta final : public hmi::connectivity::WifiStaPort {
 public:
  void set_scenario(VirtualWifiScenario scenario) noexcept;
  void force_disconnect() noexcept;

  bool begin_scan(std::uint32_t generation) override;
  [[nodiscard]] bool scan_running() const override;
  hmi::connectivity::WifiScanBatch take_scan_result() override;
  void cancel_scan() override;
  [[nodiscard]] unsigned scan_attempt() const override { return 1U; }
  void poll() override;
  bool connect(
      const hmi::connectivity::WifiStationCredential& credential) override;
  void disconnect() override;
  [[nodiscard]] hmi::connectivity::WifiLinkState link_state() const override {
    return link_;
  }
  [[nodiscard]] bool has_ip() const override;
  void copy_ipv4(char* output, std::size_t capacity) const override;
  bool begin_signal_sample(std::uint32_t generation) override;
  [[nodiscard]] bool signal_sample_running() const override { return false; }
  hmi::connectivity::WifiSignalSample take_signal_sample() override;

 private:
  VirtualWifiScenario scenario_ = VirtualWifiScenario::normal;
  std::uint32_t scan_generation_ = 0U;
  unsigned scan_ticks_ = 0U;
  unsigned connect_ticks_ = 0U;
  unsigned connected_ticks_ = 0U;
  bool scan_cancelled_ = false;
  bool ip_ = false;
  hmi::connectivity::WifiLinkState link_ =
      hmi::connectivity::WifiLinkState::idle;
  hmi::connectivity::WifiLinkState connect_outcome_ =
      hmi::connectivity::WifiLinkState::idle;
  hmi::connectivity::WifiSignalSample signal_sample_{};
  std::int16_t connected_rssi_ = 0;
};

/** Windows process memory에만 credential 하나를 보관하는 실패 주입 가능 store다. */
class MemoryWifiStationSettingsStore final
    : public hmi::connectivity::WifiStationSettingsStore {
 public:
  ~MemoryWifiStationSettingsStore() override;

  void set_fail_load(bool value) noexcept { fail_load_ = value; }
  void set_fail_save(bool value) noexcept { fail_save_ = value; }
  void set_fail_erase(bool value) noexcept { fail_erase_ = value; }
  [[nodiscard]] unsigned save_count() const noexcept { return save_count_; }

  hmi::connectivity::WifiStationSettingsLoadStatus load(
      hmi::connectivity::WifiStationCredential& output) override;
  bool save(
      const hmi::connectivity::WifiStationCredential& credential) override;
  bool erase() override;

 private:
  hmi::connectivity::WifiStationCredential saved_{};
  bool present_ = false;
  bool fail_load_ = false;
  bool fail_save_ = false;
  bool fail_erase_ = false;
  unsigned save_count_ = 0U;
};

}  // namespace hmi::platform::connectivity
