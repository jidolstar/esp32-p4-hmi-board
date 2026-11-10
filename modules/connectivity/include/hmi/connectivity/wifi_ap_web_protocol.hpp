#pragma once

#include <cstddef>
#include <cstdint>

namespace hmi::connectivity {

inline constexpr std::size_t kWifiApWebTokenHexBytes = 32U;
inline constexpr std::size_t kWifiApWebMaximumFrameBytes = 256U;
inline constexpr std::size_t kWifiApWebMaximumClients = 2U;
inline constexpr std::size_t kWifiApWebCommandQueueCapacity = 8U;
inline constexpr std::size_t kWifiApWebMaximumReplyBytes = 256U;

enum class WifiApWebHttpRoute : std::uint8_t {
  bundle,
  favicon,
  health,
  websocket,
  not_found,
  method_not_allowed,
  bad_upgrade,
};

/** HTTP adapter가 method/path/upgrade만으로 결정하는 고정 route 정책이다. */
[[nodiscard]] WifiApWebHttpRoute wifi_ap_web_route(
    const char* method, const char* path, bool websocket_upgrade) noexcept;

enum class WifiApWebFrameStatus : std::uint8_t {
  authenticated,
  queued,
  denied,
  malformed,
  frame_too_large,
  queue_full,
  unknown_client,
  inactive,
};

struct WifiApWebRandomPort {
  using Fill = bool (*)(void* context, std::uint8_t* output,
                        std::size_t size) noexcept;
  void* context = nullptr;
  Fill fill = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return fill != nullptr;
  }
};

/** host가 제공하는 장치 suffix와 CSPRNG의 비소유 view다. */
struct WifiApWebSessionSource {
  WifiApWebRandomPort random{};
  char device_suffix[5]{};
};

static_assert(sizeof(WifiApWebRandomPort) <= sizeof(void*) * 2U,
              "random port must remain a two-pointer view");
static_assert(sizeof(WifiApWebSessionSource) <= 32U,
              "session source must remain a small non-owning view");

}  // namespace hmi::connectivity
