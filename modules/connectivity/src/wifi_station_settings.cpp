#include "hmi/connectivity/wifi_station_settings.hpp"

#include <cstring>

namespace hmi::connectivity {
namespace {

bool is_printable_ascii(const std::uint8_t value) noexcept {
  return value >= 0x20U && value <= 0x7EU;
}

bool is_hex_ascii(const std::uint8_t value) noexcept {
  return (value >= '0' && value <= '9') ||
         (value >= 'a' && value <= 'f') ||
         (value >= 'A' && value <= 'F');
}

CredentialValidationError validate_bytes(
    const std::uint8_t* ssid, const std::size_t ssid_length,
    const std::uint8_t* password, const std::size_t password_length,
    const CredentialInputSource source) noexcept {
  if (ssid == nullptr || ssid_length == 0U) {
    return CredentialValidationError::empty_ssid;
  }
  if (ssid_length > kMaximumSsidBytes) {
    return CredentialValidationError::ssid_too_long;
  }
  for (std::size_t index = 0U; index < ssid_length; ++index) {
    if (ssid[index] == 0U) {
      return CredentialValidationError::ssid_contains_null;
    }
    if (source == CredentialInputSource::hmi_touch &&
        !is_printable_ascii(ssid[index])) {
      return CredentialValidationError::ssid_not_printable_ascii;
    }
  }

  // 빈 password는 open network다. WPA2 passphrase는 8~63 ASCII 또는 64자리 PSK다.
  if (password_length != 0U &&
      (password_length < 8U || password_length > kMaximumPasswordBytes)) {
    return CredentialValidationError::password_invalid_length;
  }
  if (password_length != 0U && password == nullptr) {
    return CredentialValidationError::password_invalid_length;
  }
  for (std::size_t index = 0U; index < password_length; ++index) {
    if (password[index] == 0U) {
      return CredentialValidationError::password_contains_null;
    }
    if (!is_printable_ascii(password[index])) {
      return CredentialValidationError::password_not_printable_ascii;
    }
    if (password_length == kMaximumPasswordBytes &&
        !is_hex_ascii(password[index])) {
      return CredentialValidationError::password_64_not_hex;
    }
  }
  return CredentialValidationError::none;
}

}  // namespace

void clear_wifi_station_credential(
    WifiStationCredential& credential) noexcept {
  auto* bytes = reinterpret_cast<volatile unsigned char*>(&credential);
  for (std::size_t index = 0U; index < sizeof(credential); ++index) {
    bytes[index] = 0U;
  }
}

CredentialValidationError make_wifi_station_credential(
    const std::uint8_t* ssid, const std::size_t ssid_length,
    const std::uint8_t* password, const std::size_t password_length,
    const CredentialInputSource source,
    WifiStationCredential& output) noexcept {
  const CredentialValidationError validation =
      validate_bytes(ssid, ssid_length, password, password_length, source);
  if (validation != CredentialValidationError::none) {
    return validation;
  }

  WifiStationCredential candidate{};
  std::memcpy(candidate.ssid, ssid, ssid_length);
  if (password_length != 0U) {
    std::memcpy(candidate.password, password, password_length);
  }
  candidate.ssid_length = static_cast<std::uint8_t>(ssid_length);
  candidate.password_length = static_cast<std::uint8_t>(password_length);
  output = candidate;
  clear_wifi_station_credential(candidate);
  return CredentialValidationError::none;
}

CredentialValidationError validate_wifi_station_credential(
    const WifiStationCredential& credential,
    const CredentialInputSource source) noexcept {
  if (credential.ssid_length > kMaximumSsidBytes ||
      credential.password_length > kMaximumPasswordBytes ||
      credential.ssid[credential.ssid_length] != '\0' ||
      credential.password[credential.password_length] != '\0') {
    return CredentialValidationError::malformed_record;
  }
  return validate_bytes(
      reinterpret_cast<const std::uint8_t*>(credential.ssid),
      credential.ssid_length,
      reinterpret_cast<const std::uint8_t*>(credential.password),
      credential.password_length, source);
}

bool format_wifi_ssid_for_log(const WifiStationCredential& credential,
                              char* output,
                              const std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0U ||
      credential.ssid_length > kMaximumSsidBytes) {
    return false;
  }

  constexpr char kHex[] = "0123456789ABCDEF";
  std::size_t required = 1U;
  for (std::size_t index = 0U; index < credential.ssid_length; ++index) {
    const auto value = static_cast<std::uint8_t>(credential.ssid[index]);
    const bool escape = value < 0x20U || value == 0x7FU || value == '"' ||
                        value == '\\';
    required += escape ? 4U : 1U;
  }
  if (required > capacity) {
    output[0] = '\0';
    return false;
  }

  std::size_t cursor = 0U;
  for (std::size_t index = 0U; index < credential.ssid_length; ++index) {
    const auto value = static_cast<std::uint8_t>(credential.ssid[index]);
    const bool escape = value < 0x20U || value == 0x7FU || value == '"' ||
                        value == '\\';
    if (!escape) {
      output[cursor++] = static_cast<char>(value);
      continue;
    }
    output[cursor++] = '\\';
    output[cursor++] = 'x';
    output[cursor++] = kHex[(value >> 4U) & 0x0FU];
    output[cursor++] = kHex[value & 0x0FU];
  }
  output[cursor] = '\0';
  return true;
}

}  // namespace hmi::connectivity
