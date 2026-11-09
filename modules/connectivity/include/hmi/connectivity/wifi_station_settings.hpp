#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/connectivity_types.hpp"

namespace hmi::connectivity {

inline constexpr std::size_t kMaximumLoggedSsidBytes =
    kMaximumSsidBytes * 4U + 1U;

/** compiler가 생략할 수 없는 overwrite로 임시 credential의 비밀을 정리한다. */
void clear_wifi_station_credential(
    WifiStationCredential& credential) noexcept;

/** 검증된 byte만 null 종단 bounded credential로 복사한다. */
CredentialValidationError make_wifi_station_credential(
    const std::uint8_t* ssid, std::size_t ssid_length,
    const std::uint8_t* password, std::size_t password_length,
    CredentialInputSource source, WifiStationCredential& output) noexcept;

/** store에서 복원한 credential의 길이, 종단과 문자 정책을 다시 검증한다. */
CredentialValidationError validate_wifi_station_credential(
    const WifiStationCredential& credential,
    CredentialInputSource source) noexcept;

/**
 * SSID를 password 노출 없이 한 줄 log용 문자열로 변환한다.
 *
 * 제어문자, 따옴표와 역슬래시는 `\\xNN`으로 치환한다.
 */
bool format_wifi_ssid_for_log(const WifiStationCredential& credential,
                              char* output,
                              std::size_t capacity) noexcept;

}  // namespace hmi::connectivity
