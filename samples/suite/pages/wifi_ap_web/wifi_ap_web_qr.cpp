#include "pages/wifi_ap_web/wifi_ap_web_qr.hpp"

#include <cstring>

namespace hmi::samples::suite::wifi_ap_web {
namespace {

struct BoundedWriter {
  char* output = nullptr;
  std::size_t capacity = 0U;
  std::size_t size = 0U;

  bool append(const char value) noexcept {
    if (output == nullptr || capacity == 0U || size + 1U >= capacity ||
        size + 1U > kMaximumQrPayloadBytes) {
      return false;
    }
    output[size++] = value;
    output[size] = '\0';
    return true;
  }

  bool append(const char* text) noexcept {
    if (text == nullptr) {
      return false;
    }
    while (*text != '\0') {
      if (!append(*text++)) {
        return false;
      }
    }
    return true;
  }
};

bool requires_wifi_escape(const char value) noexcept {
  return value == '\\' || value == ';' || value == ',' || value == ':' ||
         value == '"';
}

bool append_wifi_text(BoundedWriter& writer, const char* text) noexcept {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  while (*text != '\0') {
    if (requires_wifi_escape(*text) && !writer.append('\\')) {
      return false;
    }
    if (!writer.append(*text++)) {
      return false;
    }
  }
  return true;
}

bool valid_ipv4_text(const char* ipv4) noexcept {
  if (ipv4 == nullptr) {
    return false;
  }
  const std::size_t size = std::strlen(ipv4);
  if (size < 7U || size > 15U) {
    return false;
  }
  for (std::size_t index = 0U; index < size; ++index) {
    if ((ipv4[index] < '0' || ipv4[index] > '9') && ipv4[index] != '.') {
      return false;
    }
  }
  return true;
}

bool valid_hex_token(const char* token) noexcept {
  if (token == nullptr || std::strlen(token) != 32U) {
    return false;
  }
  for (std::size_t index = 0U; index < 32U; ++index) {
    const char value = token[index];
    if (!((value >= '0' && value <= '9') ||
          (value >= 'A' && value <= 'F') ||
          (value >= 'a' && value <= 'f'))) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool make_wifi_qr_payload(const char* ssid, const char* password, char* output,
                          const std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0U) {
    return false;
  }
  output[0] = '\0';
  BoundedWriter writer{output, capacity, 0U};
  return writer.append("WIFI:T:WPA;S:") && append_wifi_text(writer, ssid) &&
         writer.append(";P:") && append_wifi_text(writer, password) &&
         writer.append(";;");
}

bool make_control_url_qr_payload(const char* ipv4, const char* token,
                                 char* output,
                                 const std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0U) {
    return false;
  }
  output[0] = '\0';
  if (!valid_ipv4_text(ipv4) || !valid_hex_token(token)) {
    return false;
  }
  BoundedWriter writer{output, capacity, 0U};
  return writer.append("http://") && writer.append(ipv4) &&
         writer.append("/#token=") && writer.append(token);
}

}  // namespace hmi::samples::suite::wifi_ap_web
