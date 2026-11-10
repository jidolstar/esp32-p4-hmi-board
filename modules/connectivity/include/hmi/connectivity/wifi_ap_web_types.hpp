#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/wifi_ap_runtime.hpp"
#include "hmi/connectivity/wifi_ap_web_protocol.hpp"

namespace hmi::connectivity {

inline constexpr std::size_t kWifiApWebSwitchCount = 4U;

/** Web/HMI가 공유하는 고정 switch 식별자다. 외부 protocol과 맞춰 1부터 시작한다. */
enum class WifiApWebSwitchId : std::uint8_t {
  one = 1U,
  two = 2U,
  three = 3U,
  four = 4U,
};

/** transport가 controller에 전달하는 revision 기반 변경 명령이다. */
struct WifiApWebSwitchCommand {
  std::uint32_t expected_revision = 0U;
  std::uint8_t switch_id = 0U;
  bool value = false;
};

enum class WifiApWebCommandStatus : std::uint8_t {
  applied,
  unchanged,
  invalid_switch,
  inactive,
  stale_revision,
  revision_exhausted,
};

/** 명령 처리 뒤 transport가 응답을 만들 수 있는 작은 결과다. */
struct WifiApWebCommandResult {
  std::uint32_t revision = 0U;
  WifiApWebCommandStatus status = WifiApWebCommandStatus::inactive;
  std::uint8_t switch_id = 0U;
  bool value = false;
};

/** password/token을 포함하지 않는 controller의 canonical diagnostic snapshot이다. */
struct WifiApWebSnapshot {
  std::uint32_t revision = 0U;
  WifiApRuntimeSnapshot runtime{};
  bool switch_values[kWifiApWebSwitchCount]{};
  std::uint8_t authenticated_client_count = 0U;
};

static_assert(sizeof(WifiApWebSwitchCommand) <= 12U,
              "AP web command must stay bounded");
static_assert(sizeof(WifiApWebCommandResult) <= 12U,
              "AP web command result must stay bounded");
static_assert(sizeof(WifiApWebSnapshot) <= 24U,
              "AP web snapshot must stay small and secret-free");

}  // namespace hmi::connectivity
