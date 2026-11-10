#pragma once

#include <cstddef>

namespace hmi::samples::suite::wifi_ap_web {

inline constexpr std::size_t kMaximumQrPayloadBytes = 192U;

/** WPA AP 접속용 표준 Wi-Fi QR payload를 고정 버퍼에 만든다. */
bool make_wifi_qr_payload(const char* ssid, const char* password, char* output,
                          std::size_t capacity) noexcept;

/** token을 HTTP request가 아닌 URL fragment로 전달하는 제어 URL을 만든다. */
bool make_control_url_qr_payload(const char* ipv4, const char* token,
                                 char* output,
                                 std::size_t capacity) noexcept;

}  // namespace hmi::samples::suite::wifi_ap_web
