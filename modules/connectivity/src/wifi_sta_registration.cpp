#include "hmi/connectivity/wifi_sta_registration.hpp"

#include <cstring>

#include "hmi/connectivity/wifi_station_settings.hpp"

namespace hmi::connectivity {
namespace {

constexpr std::uint32_t kConnectTimeoutMs = 20000U;
constexpr std::uint32_t kSignalIntervalMs = 5000U;
constexpr std::uint32_t kSignalStaleMs = 15000U;

WifiRegistrationError link_error(const WifiLinkState state) noexcept {
  switch (state) {
    case WifiLinkState::no_access_point:
      return WifiRegistrationError::no_access_point;
    case WifiLinkState::authentication_failed:
      return WifiRegistrationError::authentication_failed;
    default:
      return WifiRegistrationError::driver_start;
  }
}

}  // namespace

WifiStaRegistration::~WifiStaRegistration() { clear_current(); }

void WifiStaRegistration::clear_current() noexcept {
  clear_wifi_station_credential(current_);
  snapshot_.current_ssid[0] = '\0';
}

bool WifiStaRegistration::begin(
    const std::uint32_t now_ms,
    const bool reuse_saved_credentials) noexcept {
  if (initialized_) {
    return false;
  }
  initialized_ = true;
  reuse_saved_ = reuse_saved_credentials;
  last_now_ms_ = now_ms;
  if (!reuse_saved_) {
    return true;
  }

  WifiStationCredential saved{};
  const WifiStationSettingsLoadStatus result = store_.load(saved);
  if (result == WifiStationSettingsLoadStatus::loaded &&
      validate_wifi_station_credential(saved,
                                       CredentialInputSource::service_tool) ==
          CredentialValidationError::none) {
    current_ = saved;
    clear_wifi_station_credential(saved);
    snapshot_.has_saved_credential = true;
    start_connection(now_ms, false);
    return true;
  }
  clear_wifi_station_credential(saved);
  if (result != WifiStationSettingsLoadStatus::not_found) {
    snapshot_.error = WifiRegistrationError::storage_load;
  }
  return true;
}

bool WifiStaRegistration::busy() const noexcept {
  return scan_.busy() || scan_.snapshot().draining ||
         snapshot_.state == WifiRegistrationState::connecting ||
         snapshot_.state == WifiRegistrationState::restoring;
}

bool WifiStaRegistration::request_scan() noexcept {
  if (!initialized_ || busy() ||
      snapshot_.state == WifiRegistrationState::connected_not_saved) {
    return false;
  }
  if (!scan_.request()) {
    return false;
  }
  if (snapshot_.state != WifiRegistrationState::saved) {
    snapshot_.state = WifiRegistrationState::scanning;
  }
  snapshot_.error = WifiRegistrationError::none;
  return true;
}

bool WifiStaRegistration::connect_bytes(
    const std::uint8_t* ssid, const std::size_t ssid_length,
    const std::uint8_t* password, const std::size_t password_length,
    const WifiSecurity security,
    const CredentialInputSource source) noexcept {
  if (!initialized_ || busy() ||
      snapshot_.state == WifiRegistrationState::connected_not_saved) {
    return false;
  }
  if (security == WifiSecurity::unsupported) {
    snapshot_.error = WifiRegistrationError::unsupported_security;
    return false;
  }
  if ((security == WifiSecurity::open && password_length != 0U) ||
      (security != WifiSecurity::open && password_length == 0U) ||
      (security == WifiSecurity::wpa3_personal &&
       password_length == kMaximumPasswordBytes)) {
    snapshot_.validation =
        CredentialValidationError::password_invalid_length;
    snapshot_.error = WifiRegistrationError::invalid_credential;
    return false;
  }

  WifiStationCredential candidate{};
  snapshot_.validation = make_wifi_station_credential(
      ssid, ssid_length, password, password_length, source, candidate);
  if (snapshot_.validation != CredentialValidationError::none) {
    clear_wifi_station_credential(candidate);
    snapshot_.error = WifiRegistrationError::invalid_credential;
    return false;
  }
  clear_current();
  current_ = candidate;
  clear_wifi_station_credential(candidate);
  temporary_ = true;
  cancelled_ = false;
  snapshot_.error = WifiRegistrationError::none;
  start_connection(last_now_ms_, false);
  return true;
}

bool WifiStaRegistration::connect_hidden(
    const std::uint8_t* ssid, const std::size_t ssid_length,
    const std::uint8_t* password, const std::size_t password_length,
    const WifiSecurity security) noexcept {
  return connect_bytes(ssid, ssid_length, password, password_length, security,
                       CredentialInputSource::hmi_touch);
}

bool WifiStaRegistration::connect_service(
    const std::uint8_t* ssid, const std::size_t ssid_length,
    const std::uint8_t* password, const std::size_t password_length,
    const WifiSecurity security) noexcept {
  return connect_bytes(ssid, ssid_length, password, password_length, security,
                       CredentialInputSource::service_tool);
}

bool WifiStaRegistration::connect_selected(
    const std::uint32_t generation, const std::size_t index,
    const std::uint8_t* password,
    const std::size_t password_length) noexcept {
  const WifiScanEntry* selected = scan_.entry(generation, index);
  if (selected == nullptr) {
    snapshot_.error = WifiRegistrationError::stale_selection;
    return false;
  }
  return connect_bytes(
      reinterpret_cast<const std::uint8_t*>(selected->ssid),
      selected->ssid_length, password, password_length, selected->security,
      CredentialInputSource::service_tool);
}

void WifiStaRegistration::start_connection(const std::uint32_t now_ms,
                                           const bool restoring) noexcept {
  port_.disconnect();
  ++signal_generation_;
  signal_connected_ = false;
  signal_attempted_ = false;
  snapshot_.signal_level = 0U;
  snapshot_.link_connected = false;
  snapshot_.has_ip = false;
  snapshot_.ipv4[0] = '\0';
  std::memcpy(snapshot_.current_ssid, current_.ssid, current_.ssid_length);
  snapshot_.current_ssid[current_.ssid_length] = '\0';
  state_started_ms_ = now_ms;
  restoring_ = restoring;
  snapshot_.restoring_previous = restoring;
  if (!port_.connect(current_)) {
    snapshot_.error = restoring ? WifiRegistrationError::restore_failed
                                : WifiRegistrationError::driver_start;
    if (temporary_ && !restoring) {
      restore_previous(now_ms, false);
    } else {
      snapshot_.state = WifiRegistrationState::failed;
    }
    return;
  }
  snapshot_.state = restoring ? WifiRegistrationState::restoring
                              : WifiRegistrationState::connecting;
}

void WifiStaRegistration::restore_previous(const std::uint32_t now_ms,
                                           const bool cancelled) noexcept {
  // 완료된 목록은 유지해 같은 AP에 암호만 고쳐 재시도할 수 있게 한다.
  if (scan_.busy()) {
    scan_.cancel();
  }
  port_.disconnect();
  clear_current();
  temporary_ = false;
  cancelled_ = cancelled;
  snapshot_.link_connected = false;
  snapshot_.has_ip = false;
  snapshot_.ipv4[0] = '\0';
  snapshot_.restoring_previous = false;
  if (cancelled) {
    snapshot_.error = WifiRegistrationError::none;
  }

  WifiStationCredential saved{};
  const WifiStationSettingsLoadStatus load =
      reuse_saved_ && snapshot_.has_saved_credential
          ? store_.load(saved)
          : WifiStationSettingsLoadStatus::not_found;
  if (load == WifiStationSettingsLoadStatus::loaded &&
      validate_wifi_station_credential(saved,
                                       CredentialInputSource::service_tool) ==
          CredentialValidationError::none) {
    current_ = saved;
    clear_wifi_station_credential(saved);
    start_connection(now_ms, true);
    return;
  }
  clear_wifi_station_credential(saved);
  if (load != WifiStationSettingsLoadStatus::not_found) {
    snapshot_.error = WifiRegistrationError::restore_failed;
  }
  snapshot_.state = cancelled ? WifiRegistrationState::cancelled
                              : WifiRegistrationState::failed;
}

void WifiStaRegistration::cancel() noexcept {
  scan_.cancel();
  if (temporary_ || restoring_) {
    restore_previous(last_now_ms_, true);
  } else if (snapshot_.state == WifiRegistrationState::scanning) {
    snapshot_.state = WifiRegistrationState::cancelled;
  }
}

bool WifiStaRegistration::retry_save() noexcept {
  if (snapshot_.state != WifiRegistrationState::connected_not_saved ||
      port_.link_state() != WifiLinkState::connected || !port_.has_ip()) {
    return false;
  }
  if (!store_.save(current_)) {
    return false;
  }
  temporary_ = false;
  reuse_saved_ = true;
  snapshot_.has_saved_credential = true;
  snapshot_.error = WifiRegistrationError::none;
  snapshot_.state = WifiRegistrationState::saved;
  return true;
}

bool WifiStaRegistration::forget() noexcept {
  if (!initialized_ || busy()) {
    return false;
  }
  if (!store_.erase()) {
    snapshot_.error = WifiRegistrationError::storage_erase;
    return false;
  }
  scan_.cancel();
  port_.disconnect();
  ++signal_generation_;
  signal_connected_ = false;
  signal_attempted_ = false;
  clear_current();
  temporary_ = false;
  snapshot_ = {};
  snapshot_.state = WifiRegistrationState::forgotten;
  return true;
}

void WifiStaRegistration::tick(const std::uint32_t now_ms) noexcept {
  last_now_ms_ = now_ms;
  port_.poll();
  if (!initialized_) {
    return;
  }
  tick_signal(now_ms);
  scan_.tick(now_ms,
             snapshot_.state != WifiRegistrationState::connecting &&
                 snapshot_.state != WifiRegistrationState::restoring);

  if (snapshot_.state == WifiRegistrationState::saved) {
    snapshot_.link_connected =
        port_.link_state() == WifiLinkState::connected;
    snapshot_.has_ip = snapshot_.link_connected && port_.has_ip();
    if (!snapshot_.has_ip) {
      snapshot_.state = WifiRegistrationState::failed;
      snapshot_.error = WifiRegistrationError::driver_start;
      snapshot_.ipv4[0] = '\0';
    }
  }
  if (snapshot_.state == WifiRegistrationState::scanning && !scan_.busy()) {
    const WifiScanSnapshot scan = scan_.snapshot();
    if (scan.state == WifiScanState::ready) {
      snapshot_.state = snapshot_.has_saved_credential && port_.has_ip()
                            ? WifiRegistrationState::saved
                            : WifiRegistrationState::idle;
    } else if (scan.state == WifiScanState::failed ||
               scan.state == WifiScanState::timed_out) {
      snapshot_.state = WifiRegistrationState::failed;
      snapshot_.error = scan.state == WifiScanState::timed_out
                            ? WifiRegistrationError::timeout
                            : WifiRegistrationError::driver_start;
    }
  }
  if (snapshot_.state != WifiRegistrationState::connecting &&
      snapshot_.state != WifiRegistrationState::restoring) {
    return;
  }

  const WifiLinkState link = port_.link_state();
  snapshot_.link_connected = link == WifiLinkState::connected;
  snapshot_.has_ip = snapshot_.link_connected && port_.has_ip();
  if (snapshot_.has_ip) {
    port_.copy_ipv4(snapshot_.ipv4, sizeof(snapshot_.ipv4));
    if (restoring_) {
      restoring_ = false;
      snapshot_.restoring_previous = false;
      snapshot_.state = cancelled_ ? WifiRegistrationState::cancelled
                                   : WifiRegistrationState::failed;
    } else if (temporary_) {
      if (store_.save(current_)) {
        temporary_ = false;
        reuse_saved_ = true;
        snapshot_.has_saved_credential = true;
        snapshot_.state = WifiRegistrationState::saved;
      } else {
        snapshot_.error = WifiRegistrationError::storage_save;
        snapshot_.state = WifiRegistrationState::connected_not_saved;
      }
    } else {
      snapshot_.state = WifiRegistrationState::saved;
    }
    return;
  }

  if (link == WifiLinkState::no_access_point ||
      link == WifiLinkState::authentication_failed ||
      link == WifiLinkState::failed ||
      now_ms - state_started_ms_ >= kConnectTimeoutMs) {
    snapshot_.error =
        link == WifiLinkState::connecting || link == WifiLinkState::connected
            ? WifiRegistrationError::timeout
            : link_error(link);
    if (temporary_) {
      restore_previous(now_ms, false);
    } else {
      restoring_ = false;
      snapshot_.restoring_previous = false;
      if (cancelled_) {
        snapshot_.error = WifiRegistrationError::restore_failed;
      }
      snapshot_.state = WifiRegistrationState::failed;
    }
  }
}

std::uint8_t WifiStaRegistration::level_for_rssi(
    const std::int16_t rssi) noexcept {
  if (rssi >= 0 || rssi < -126) {
    return 0U;
  }
  return rssi >= -55 ? 4U : rssi >= -67 ? 3U : rssi >= -75 ? 2U : 1U;
}

void WifiStaRegistration::tick_signal(const std::uint32_t now_ms) noexcept {
  const bool connected = port_.link_state() == WifiLinkState::connected &&
                         port_.has_ip();
  if (connected != signal_connected_) {
    signal_connected_ = connected;
    ++signal_generation_;
    signal_attempted_ = false;
    snapshot_.signal_level = 0U;
  }
  if (signal_pending_ && !port_.signal_sample_running()) {
    const WifiSignalSample sample = port_.take_signal_sample();
    signal_pending_ = false;
    if (connected && sample.valid &&
        sample.generation == signal_generation_) {
      snapshot_.signal_level = level_for_rssi(sample.rssi);
      signal_last_valid_ms_ = now_ms;
    }
  }
  if (!connected) {
    snapshot_.signal_level = 0U;
    return;
  }
  if (snapshot_.signal_level != 0U &&
      now_ms - signal_last_valid_ms_ > kSignalStaleMs) {
    snapshot_.signal_level = 0U;
  }
  if (signal_pending_ || scan_.busy() || scan_.snapshot().draining ||
      (signal_attempted_ &&
       now_ms - signal_last_attempt_ms_ < kSignalIntervalMs)) {
    return;
  }
  signal_attempted_ = true;
  signal_last_attempt_ms_ = now_ms;
  signal_pending_ = port_.begin_signal_sample(signal_generation_);
}

}  // namespace hmi::connectivity
