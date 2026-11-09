#include "connectivity/esp_hosted_wifi_sta.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <esp32-hal-hosted.h>
#include <esp_hosted_host_fw_ver.h>
#include <esp_wifi.h>
#include <esp_wifi_remote_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

namespace hmi::platform::connectivity {
namespace {

using hmi::connectivity::WifiLinkState;
using hmi::connectivity::WifiScanBatch;
using hmi::connectivity::WifiScanEntry;
using hmi::connectivity::WifiSecurity;
using hmi::connectivity::WifiSignalSample;
using hmi::connectivity::WifiStationCredential;

constexpr std::int8_t kClock = 18;
constexpr std::int8_t kCommand = 19;
constexpr std::int8_t kD0 = 14;
constexpr std::int8_t kD1 = 15;
constexpr std::int8_t kD2 = 16;
constexpr std::int8_t kD3 = 17;
constexpr std::int8_t kReset = 54;

static_assert(ESP_HOSTED_VERSION_MAJOR_1 == 2 &&
                  ESP_HOSTED_VERSION_MINOR_1 == 12 &&
                  ESP_HOSTED_VERSION_PATCH_1 == 8,
              "Review C6 companion firmware before changing ESP-Hosted");
static_assert(ESP_WIFI_REMOTE_VERSION == ESP_WIFI_REMOTE_VERSION_VAL(1, 6, 0),
              "Review Wi-Fi remote API before changing its version");
#if defined(CONFIG_ESP_HOSTED_SDIO_PIN_CLK) && \
    defined(CONFIG_ESP_HOSTED_SDIO_PIN_CMD) && \
    defined(CONFIG_ESP_HOSTED_SDIO_PIN_D0) && \
    defined(CONFIG_ESP_HOSTED_SDIO_PIN_D1) && \
    defined(CONFIG_ESP_HOSTED_SDIO_PIN_D2) && \
    defined(CONFIG_ESP_HOSTED_SDIO_PIN_D3) && \
    defined(CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE)
static_assert(CONFIG_ESP_HOSTED_SDIO_PIN_CLK == kClock &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_CMD == kCommand &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D0 == kD0 &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D1 == kD1 &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D2 == kD2 &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D3 == kD3 &&
                  CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE == kReset,
              "ESP-Hosted SDIO pins must match the JC1060P470C wiring");
#endif

bool ensure_station() {
  static bool pins_ready = false;
  if (!pins_ready) {
    if (!WiFi.setPins(kClock, kCommand, kD0, kD1, kD2, kD3, kReset)) {
      return false;
    }
    pins_ready = true;
  }
  if (!WiFi.mode(WIFI_STA)) {
    return false;
  }
  WiFi.setAutoReconnect(false);
  return WiFi.STA.begin(false);
}

WifiSecurity security_of(const wifi_auth_mode_t mode) noexcept {
  switch (mode) {
    case WIFI_AUTH_OPEN:
      return WifiSecurity::open;
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return WifiSecurity::personal;
    case WIFI_AUTH_WPA3_PSK:
      return WifiSecurity::wpa3_personal;
    default:
      return WifiSecurity::unsupported;
  }
}

/** worker가 자신의 참조를 가져 화면 취소 뒤에도 batch 정리까지 안전하게 끝난다. */
struct ScanWork {
  std::atomic<unsigned> references{1U};
  std::atomic<bool> running{true};
  std::atomic<bool> cancelled{false};
  std::atomic<unsigned> attempt{0U};
  WifiScanBatch result{};

  void release() noexcept {
    if (references.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
      delete this;
    }
  }

  static void entry(void* context) {
    auto* self = static_cast<ScanWork*>(context);
    self->run();
    self->running.store(false, std::memory_order_release);
    self->release();
    vTaskDeleteWithCaps(nullptr);
  }

  void run() {
    if (cancelled.load() || !ensure_station()) {
      return;
    }
    wifi_scan_config_t config{};
    config.show_hidden = true;
    config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    config.scan_time.active.min = 120U;
    config.scan_time.active.max = 300U;
    for (unsigned pass = 0U; pass < 2U && !cancelled.load(); ++pass) {
      attempt.store(pass + 1U);
      std::uint16_t found = 0U;
      const esp_err_t started = esp_wifi_scan_start(&config, true);
      const esp_err_t counted =
          started == ESP_OK ? esp_wifi_scan_get_ap_num(&found) : started;
      if (cancelled.load()) {
        esp_wifi_clear_ap_list();
        return;
      }
      if (counted == ESP_OK && found != 0U) {
        const std::uint16_t count = std::min<std::uint16_t>(
            found, hmi::connectivity::kMaximumWifiScanEntries);
        result.entries.reset(new (std::nothrow) WifiScanEntry[count]{});
        if (!result.entries) {
          esp_wifi_clear_ap_list();
          return;
        }
        for (std::uint16_t index = 0U; index < count; ++index) {
          wifi_ap_record_t record{};
          if (cancelled.load() ||
              esp_wifi_scan_get_ap_record(&record) != ESP_OK) {
            esp_wifi_clear_ap_list();
            result.entries.reset();
            return;
          }
          WifiScanEntry& item = result.entries[index];
          item.ssid_length = static_cast<std::uint8_t>(strnlen(
              reinterpret_cast<const char*>(record.ssid),
              hmi::connectivity::kMaximumSsidBytes));
          std::memcpy(item.ssid, record.ssid, item.ssid_length);
          std::memcpy(item.bssid, record.bssid, sizeof(item.bssid));
          item.rssi = record.rssi;
          item.channel = record.primary;
          item.security = security_of(record.authmode);
        }
        esp_wifi_clear_ap_list();
        result.count = count;
        result.truncated = found > count;
        result.succeeded = true;
        return;
      }
      esp_wifi_clear_ap_list();
      if (pass == 1U) {
        result.succeeded = counted == ESP_OK;
        return;
      }
      for (unsigned pause = 0U; pause < 25U && !cancelled.load(); ++pause) {
        delay(20U);
      }
    }
  }
};

static_assert(sizeof(ScanWork) <= 64U,
              "RF work must not keep a scan catalog on task stack");
static_assert(sizeof(wifi_ap_record_t) <= 512U,
              "worker keeps one SDK AP record on stack at a time");

/** RSSI 조회 worker는 generation을 포함한 작은 결과 하나만 보관한다. */
struct SignalWork {
  std::atomic<unsigned> references{1U};
  std::atomic<bool> running{true};
  WifiSignalSample result{};

  void release() noexcept {
    if (references.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
      delete this;
    }
  }

  static void entry(void* context) {
    auto* self = static_cast<SignalWork*>(context);
    wifi_ap_record_t record{};
    self->result.valid = esp_wifi_sta_get_ap_info(&record) == ESP_OK;
    if (self->result.valid) {
      self->result.rssi = record.rssi;
    }
    self->running.store(false, std::memory_order_release);
    self->release();
    vTaskDeleteWithCaps(nullptr);
  }
};

static_assert(sizeof(SignalWork) <= 24U,
              "RF signal worker keeps only one sample");

}  // namespace

class EspHostedWifiSta::Impl {
 public:
  ~Impl() {
    if (event_handler_ != 0U) {
      WiFi.removeEvent(event_handler_);
    }
    if (scan_ != nullptr) {
      scan_->cancelled.store(true);
      scan_->release();
    }
    if (signal_ != nullptr) {
      signal_->release();
    }
  }

  bool begin_scan(const std::uint32_t generation) {
    if (connection_active_.load() || scan_running()) {
      return false;
    }
    if (scan_ != nullptr) {
      scan_->release();
      scan_ = nullptr;
    }
    scan_ = new (std::nothrow) ScanWork();
    if (scan_ == nullptr) {
      return false;
    }
    scan_->result.generation = generation;
    scan_->references.fetch_add(1U);
    if (xTaskCreateWithCaps(ScanWork::entry, "wifi-scan", 4096U, scan_, 1U,
                            nullptr, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) !=
        pdPASS) {
      scan_->running.store(false);
      scan_->release();
      scan_->release();
      scan_ = nullptr;
      return false;
    }
    return true;
  }

  [[nodiscard]] bool scan_running() const {
    return scan_ != nullptr &&
           scan_->running.load(std::memory_order_acquire);
  }

  WifiScanBatch take_scan_result() {
    return scan_ != nullptr && !scan_running() ? std::move(scan_->result)
                                               : WifiScanBatch{};
  }

  void cancel_scan() {
    if (scan_ != nullptr) {
      scan_->cancelled.store(true);
    }
  }

  [[nodiscard]] unsigned scan_attempt() const {
    return scan_ == nullptr ? 0U : scan_->attempt.load();
  }

  bool begin_signal_sample(const std::uint32_t generation) {
    if (signal_sample_running() || scan_running() ||
        connection_active_.load() || !has_ip()) {
      return false;
    }
    if (signal_ != nullptr) {
      signal_->release();
      signal_ = nullptr;
    }
    signal_ = new (std::nothrow) SignalWork();
    if (signal_ == nullptr) {
      return false;
    }
    signal_->result.generation = generation;
    signal_->references.fetch_add(1U);
    if (xTaskCreateWithCaps(SignalWork::entry, "wifi-signal", 4096U, signal_,
                            1U, nullptr,
                            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) != pdPASS) {
      signal_->running.store(false);
      signal_->release();
      signal_->release();
      signal_ = nullptr;
      return false;
    }
    return true;
  }

  [[nodiscard]] bool signal_sample_running() const {
    return signal_ != nullptr &&
           signal_->running.load(std::memory_order_acquire);
  }

  WifiSignalSample take_signal_sample() {
    if (signal_ == nullptr || signal_sample_running()) {
      return {};
    }
    const WifiSignalSample result = signal_->result;
    signal_->release();
    signal_ = nullptr;
    return result;
  }

  bool connect(const WifiStationCredential& credential) {
    if (scan_running() || !ensure_station() || !ensure_event_handler()) {
      return false;
    }
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setMinSecurity(credential.password_length != 0U ? WIFI_AUTH_WPA2_PSK
                                                         : WIFI_AUTH_OPEN);
    connection_started_ms_ = millis();
    connection_active_.store(true);
    retry_requested_.store(false);
    no_ap_retries_.store(0U);
    retry_not_before_ms_.store(connection_started_ms_);
    const bool started = WiFi.STA.connect(
        credential.ssid,
        credential.password_length != 0U ? credential.password : nullptr);
    if (!started) {
      connection_active_.store(false);
    }
    return started;
  }

  void disconnect() {
    const bool was_active = connection_active_.exchange(false);
    retry_requested_.store(false);
    // 화면 이탈을 막지 않도록 대기형 Arduino 호출 대신 driver에 요청만 보낸다.
    if (was_active || WiFi.STA.connected()) {
      esp_wifi_disconnect();
    }
  }

  void poll() {
    if (connection_active_.load() && retry_requested_.load() &&
        static_cast<std::int32_t>(millis() - retry_not_before_ms_.load()) >=
            0 &&
        retry_requested_.exchange(false)) {
      if (esp_wifi_connect() != ESP_OK) {
        connection_active_.store(false);
      }
    }
    if (connection_active_.load() &&
        millis() - connection_started_ms_ >= 20000U) {
      connection_active_.store(false);
    }
  }

  [[nodiscard]] WifiLinkState link_state() const {
    if (connection_active_.load()) {
      return WifiLinkState::connecting;
    }
    switch (WiFi.status()) {
      case WL_CONNECTED:
        return WifiLinkState::connected;
      case WL_NO_SSID_AVAIL:
        return WifiLinkState::no_access_point;
      case WL_CONNECT_FAILED:
        return WifiLinkState::authentication_failed;
      case WL_CONNECTION_LOST:
        return WifiLinkState::failed;
      default:
        return WifiLinkState::connecting;
    }
  }

  [[nodiscard]] bool has_ip() const {
    return WiFi.status() == WL_CONNECTED && WiFi.STA.connected() &&
           WiFi.STA.hasIP() && static_cast<std::uint32_t>(WiFi.localIP()) != 0U;
  }

  void copy_ipv4(char* output, const std::size_t capacity) const {
    if (output == nullptr || capacity == 0U) {
      return;
    }
    const auto address = WiFi.localIP();
    std::snprintf(output, capacity, "%u.%u.%u.%u", address[0], address[1],
                  address[2], address[3]);
  }

  [[nodiscard]] bool ready() const noexcept { return true; }

 private:
  bool ensure_event_handler() {
    if (event_handler_ != 0U) {
      return true;
    }
    event_handler_ = WiFi.onEvent(
        [this](const arduino_event_id_t event,
               const arduino_event_info_t info) {
          if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            connection_active_.store(false);
            retry_requested_.store(false);
            return;
          }
          if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED ||
              !connection_active_.load()) {
            return;
          }
          const auto reason = info.wifi_sta_disconnected.reason;
          if (reason == WIFI_REASON_ASSOC_LEAVE) {
            retry_requested_.store(true);
          } else if (reason == WIFI_REASON_NO_AP_FOUND &&
                     no_ap_retries_.fetch_add(1U) == 0U) {
            retry_not_before_ms_.store(millis() + 1000U);
            retry_requested_.store(true);
          } else {
            retry_requested_.store(false);
            connection_active_.store(false);
          }
        });
    return event_handler_ != 0U;
  }

  ScanWork* scan_ = nullptr;
  SignalWork* signal_ = nullptr;
  wifi_event_id_t event_handler_ = 0U;
  std::atomic<bool> connection_active_{false};
  std::atomic<bool> retry_requested_{false};
  std::atomic<unsigned> no_ap_retries_{0U};
  std::atomic<std::uint32_t> retry_not_before_ms_{0U};
  std::uint32_t connection_started_ms_ = 0U;
};

EspHostedWifiSta::EspHostedWifiSta()
    : impl_(new (std::nothrow) Impl()) {}

EspHostedWifiSta::~EspHostedWifiSta() = default;

bool EspHostedWifiSta::begin_scan(const std::uint32_t generation) {
  return impl_ != nullptr && impl_->begin_scan(generation);
}

bool EspHostedWifiSta::scan_running() const {
  return impl_ != nullptr && impl_->scan_running();
}

WifiScanBatch EspHostedWifiSta::take_scan_result() {
  return impl_ != nullptr ? impl_->take_scan_result() : WifiScanBatch{};
}

void EspHostedWifiSta::cancel_scan() {
  if (impl_ != nullptr) {
    impl_->cancel_scan();
  }
}

unsigned EspHostedWifiSta::scan_attempt() const {
  return impl_ != nullptr ? impl_->scan_attempt() : 0U;
}

void EspHostedWifiSta::poll() {
  if (impl_ != nullptr) {
    impl_->poll();
  }
}

bool EspHostedWifiSta::connect(const WifiStationCredential& credential) {
  return impl_ != nullptr && impl_->connect(credential);
}

void EspHostedWifiSta::disconnect() {
  if (impl_ != nullptr) {
    impl_->disconnect();
  }
}

WifiLinkState EspHostedWifiSta::link_state() const {
  return impl_ != nullptr ? impl_->link_state() : WifiLinkState::failed;
}

bool EspHostedWifiSta::has_ip() const {
  return impl_ != nullptr && impl_->has_ip();
}

void EspHostedWifiSta::copy_ipv4(char* output,
                                 const std::size_t capacity) const {
  if (impl_ != nullptr) {
    impl_->copy_ipv4(output, capacity);
  } else if (output != nullptr && capacity != 0U) {
    output[0] = '\0';
  }
}

bool EspHostedWifiSta::begin_signal_sample(const std::uint32_t generation) {
  return impl_ != nullptr && impl_->begin_signal_sample(generation);
}

bool EspHostedWifiSta::signal_sample_running() const {
  return impl_ != nullptr && impl_->signal_sample_running();
}

WifiSignalSample EspHostedWifiSta::take_signal_sample() {
  return impl_ != nullptr ? impl_->take_signal_sample() : WifiSignalSample{};
}

bool EspHostedWifiSta::ready() const noexcept { return impl_ != nullptr; }

}  // namespace hmi::platform::connectivity
