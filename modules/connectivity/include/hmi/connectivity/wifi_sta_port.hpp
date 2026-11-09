#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/connectivity_types.hpp"
#include "hmi/connectivity/wifi_scan_service.hpp"

namespace hmi::connectivity {

enum class WifiLinkState : std::uint8_t {
  idle,
  connecting,
  connected,
  no_access_point,
  authentication_failed,
  failed,
};

/** 연결 generation이 다른 RF 측정값은 registration core가 버린다. */
struct WifiSignalSample {
  std::uint32_t generation = 0U;
  std::int16_t rssi = 0;
  bool valid = false;
};

/**
 * 검색과 연결을 직렬화하는 비소유 플랫폼 경계다.
 *
 * connect가 받은 credential은 호출 뒤 보관하지 않는다. callback/worker는 UI 객체를
 * 참조하지 않고 작은 결과만 adapter 내부에 유지한다.
 */
class WifiStaPort : public WifiScanAdapter {
 public:
  ~WifiStaPort() override = default;
  virtual void poll() = 0;
  virtual bool connect(const WifiStationCredential& credential) = 0;
  virtual void disconnect() = 0;
  [[nodiscard]] virtual WifiLinkState link_state() const = 0;
  [[nodiscard]] virtual bool has_ip() const = 0;
  virtual void copy_ipv4(char* output, std::size_t capacity) const = 0;
  virtual bool begin_signal_sample(std::uint32_t generation) = 0;
  [[nodiscard]] virtual bool signal_sample_running() const = 0;
  virtual WifiSignalSample take_signal_sample() = 0;
};

}  // namespace hmi::connectivity
