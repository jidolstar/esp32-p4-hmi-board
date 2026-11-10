#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/connectivity_types.hpp"

namespace hmi::connectivity {

inline constexpr std::size_t kMaximumWifiApPasswordBytes = 63U;

/** AP transport가 보고하는 최소 수명 상태다. */
enum class WifiApPortState : std::uint8_t {
  off,
  starting,
  ready,
  stopping,
  failed,
};

/** 고정 크기 credential로 AP 시작 인자의 수명과 상한을 명확히 한다. */
struct WifiApConfig {
  char ssid[kMaximumSsidBytes + 1U]{};
  char password[kMaximumWifiApPasswordBytes + 1U]{};
  std::uint8_t ssid_length = 0U;
  std::uint8_t password_length = 0U;
};

/** UI가 AP 내부 객체 없이 읽는 비밀 없는 transport 상태다. */
struct WifiApPortSnapshot {
  WifiApPortState state = WifiApPortState::off;
  std::uint8_t client_count = 0U;
  bool health_server_ready = false;
};

/**
 * AP와 작은 HTTP health endpoint를 플랫폼 구현 뒤에 숨기는 비소유 경계다.
 *
 * start가 받은 config pointer는 호출 뒤 보관하지 않는다. 비밀번호는 log나 snapshot에
 * 포함하지 않으며, 구현은 stop 완료 후 transport 자원을 모두 해제해야 한다.
 */
class WifiApPort {
 public:
  virtual ~WifiApPort() = default;
  virtual bool start(const WifiApConfig& config) = 0;
  virtual void request_stop() = 0;
  virtual void poll() = 0;
  [[nodiscard]] virtual WifiApPortSnapshot snapshot() const noexcept = 0;
  virtual void copy_ipv4(char* output, std::size_t capacity) const noexcept = 0;
};

static_assert(sizeof(WifiApConfig) <= 104U,
              "AP config must remain a single bounded credential");
static_assert(sizeof(WifiApPortSnapshot) <= 8U,
              "AP transport snapshot must stay small and secret-free");

}  // namespace hmi::connectivity
