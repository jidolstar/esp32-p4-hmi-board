#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/wifi_sta_port.hpp"
#include "hmi/connectivity/wifi_station_settings_store.hpp"

namespace hmi::connectivity {

enum class WifiRegistrationState : std::uint8_t {
  idle,
  scanning,
  connecting,
  saved,
  connected_not_saved,
  restoring,
  failed,
  cancelled,
  forgotten,
};

enum class WifiRegistrationError : std::uint8_t {
  none,
  busy,
  invalid_credential,
  unsupported_security,
  stale_selection,
  driver_start,
  no_access_point,
  authentication_failed,
  timeout,
  storage_load,
  storage_save,
  storage_erase,
  restore_failed,
};

/** password와 AP 목록을 포함하지 않는 화면용 작은 상태 값이다. */
struct WifiRegistrationSnapshot {
  char current_ssid[kMaximumSsidBytes + 1U]{};
  char ipv4[16]{};
  WifiRegistrationState state = WifiRegistrationState::idle;
  WifiRegistrationError error = WifiRegistrationError::none;
  CredentialValidationError validation = CredentialValidationError::none;
  bool has_saved_credential = false;
  bool link_connected = false;
  bool has_ip = false;
  bool restoring_previous = false;
  std::uint8_t signal_level = 0U;
};

/**
 * STA 등록·저장·복구 정책의 단일 owner다.
 *
 * port와 store는 이 객체보다 오래 살아야 한다. 임시 credential 하나만 소유하고 UI에는
 * password 없는 snapshot과 generation 기반 AP 조회만 제공한다.
 */
class WifiStaRegistration {
 public:
  WifiStaRegistration(WifiStaPort& port,
                      WifiStationSettingsStore& store) noexcept
      : port_(port), store_(store), scan_(port) {}
  ~WifiStaRegistration();

  WifiStaRegistration(const WifiStaRegistration&) = delete;
  WifiStaRegistration& operator=(const WifiStaRegistration&) = delete;

  bool begin(std::uint32_t now_ms,
             bool reuse_saved_credentials) noexcept;
  void tick(std::uint32_t now_ms) noexcept;
  bool request_scan() noexcept;
  bool connect_hidden(const std::uint8_t* ssid, std::size_t ssid_length,
                      const std::uint8_t* password,
                      std::size_t password_length,
                      WifiSecurity security) noexcept;
  bool connect_service(const std::uint8_t* ssid, std::size_t ssid_length,
                       const std::uint8_t* password,
                       std::size_t password_length,
                       WifiSecurity security) noexcept;
  bool connect_selected(std::uint32_t generation, std::size_t index,
                        const std::uint8_t* password,
                        std::size_t password_length) noexcept;
  void cancel() noexcept;
  bool retry_save() noexcept;
  bool forget() noexcept;
  bool suspend_for_ap() noexcept;
  bool resume_after_ap(std::uint32_t now_ms) noexcept;
  [[nodiscard]] bool suspended_for_ap() const noexcept {
    return ap_suspended_;
  }

  [[nodiscard]] WifiRegistrationSnapshot snapshot() const noexcept {
    return snapshot_;
  }
  [[nodiscard]] WifiScanSnapshot scan_snapshot() const noexcept {
    return scan_.snapshot();
  }
  [[nodiscard]] const WifiScanEntry* scan_entry(
      std::uint32_t generation, std::size_t index) const noexcept {
    return scan_.entry(generation, index);
  }
  [[nodiscard]] bool busy() const noexcept;

 private:
  bool connect_bytes(const std::uint8_t* ssid, std::size_t ssid_length,
                     const std::uint8_t* password,
                     std::size_t password_length, WifiSecurity security,
                     CredentialInputSource source) noexcept;
  void start_connection(std::uint32_t now_ms, bool restoring) noexcept;
  void restore_previous(std::uint32_t now_ms, bool cancelled) noexcept;
  void clear_current() noexcept;
  void tick_signal(std::uint32_t now_ms) noexcept;
  static std::uint8_t level_for_rssi(std::int16_t rssi) noexcept;

  WifiStaPort& port_;
  WifiStationSettingsStore& store_;
  WifiScanService scan_;
  WifiStationCredential current_{};
  WifiRegistrationSnapshot snapshot_{};
  std::uint32_t state_started_ms_ = 0U;
  std::uint32_t last_now_ms_ = 0U;
  bool initialized_ = false;
  bool reuse_saved_ = true;
  bool temporary_ = false;
  bool restoring_ = false;
  bool cancelled_ = false;
  std::uint32_t signal_generation_ = 0U;
  std::uint32_t signal_last_attempt_ms_ = 0U;
  std::uint32_t signal_last_valid_ms_ = 0U;
  bool signal_attempted_ = false;
  bool signal_pending_ = false;
  bool signal_connected_ = false;
  WifiRegistrationState state_before_ap_ = WifiRegistrationState::idle;
  bool ap_suspended_ = false;
  bool reconnect_after_ap_ = false;
};

static_assert(sizeof(WifiRegistrationSnapshot) <= 64U,
              "UI snapshot must not contain a credential or scan catalog");
static_assert(sizeof(WifiStaRegistration) <= 256U,
              "Registration owns only one bounded credential");

}  // namespace hmi::connectivity
