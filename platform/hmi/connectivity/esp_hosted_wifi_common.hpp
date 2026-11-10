#pragma once

#include <WiFi.h>
#include <esp_hosted_host_fw_ver.h>
#include <esp_wifi_remote_version.h>

#include <cstdint>

namespace hmi::platform::connectivity::detail {

inline constexpr std::int8_t kEspHostedClock = 18;
inline constexpr std::int8_t kEspHostedCommand = 19;
inline constexpr std::int8_t kEspHostedD0 = 14;
inline constexpr std::int8_t kEspHostedD1 = 15;
inline constexpr std::int8_t kEspHostedD2 = 16;
inline constexpr std::int8_t kEspHostedD3 = 17;
inline constexpr std::int8_t kEspHostedReset = 54;

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
static_assert(CONFIG_ESP_HOSTED_SDIO_PIN_CLK == kEspHostedClock &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_CMD == kEspHostedCommand &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D0 == kEspHostedD0 &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D1 == kEspHostedD1 &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D2 == kEspHostedD2 &&
                  CONFIG_ESP_HOSTED_SDIO_PIN_D3 == kEspHostedD3 &&
                  CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE == kEspHostedReset,
              "ESP-Hosted SDIO pins must match the JC1060P470C wiring");
#endif

/** STA와 AP가 같은 ESP-Hosted pin 초기화를 공유한다. */
inline bool ensure_esp_hosted_pins() noexcept {
  static bool pins_ready = false;
  if (pins_ready) {
    return true;
  }
  pins_ready = WiFi.setPins(kEspHostedClock, kEspHostedCommand, kEspHostedD0,
                            kEspHostedD1, kEspHostedD2, kEspHostedD3,
                            kEspHostedReset);
  return pins_ready;
}

}  // namespace hmi::platform::connectivity::detail
