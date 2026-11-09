#pragma once

#include <cstddef>
#include <cstdint>

namespace hmi::connectivity {

inline constexpr std::size_t kMaximumSsidBytes = 32U;
inline constexpr std::size_t kMaximumPasswordBytes = 64U;

enum class CredentialInputSource : std::uint8_t {
  hmi_touch = 0,
  service_tool,
};

enum class CredentialValidationError : std::uint8_t {
  none = 0,
  empty_ssid,
  ssid_too_long,
  ssid_contains_null,
  ssid_not_printable_ascii,
  password_invalid_length,
  password_contains_null,
  password_not_printable_ascii,
  password_64_not_hex,
  malformed_record,
};

/**
 * Wi-Fi driver와 credential store 사이에서만 이동하는 bounded 비밀 값이다.
 *
 * length가 실제 byte 수의 기준이며 두 배열은 항상 null 종단된다. UI snapshot과 log는
 * 이 값을 소유하거나 복사하지 않는다.
 */
struct WifiStationCredential {
  char ssid[kMaximumSsidBytes + 1U]{};
  char password[kMaximumPasswordBytes + 1U]{};
  std::uint8_t ssid_length = 0U;
  std::uint8_t password_length = 0U;

  [[nodiscard]] bool empty() const noexcept { return ssid_length == 0U; }
};

static_assert(sizeof(WifiStationCredential) <= 104U,
              "Wi-Fi credential must remain a bounded NVS-sized value");

}  // namespace hmi::connectivity
