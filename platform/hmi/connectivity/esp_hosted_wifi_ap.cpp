#include "connectivity/esp_hosted_wifi_ap.hpp"

#include <NetworkServer.h>
#include <SHA1Builder.h>
#include <WiFi.h>
#include <base64.h>

#include <cstdio>
#include <cstring>
#include <new>

#include "connectivity/esp_hosted_wifi_common.hpp"
#include "hmi/connectivity/wifi_ap_web_controller.hpp"

namespace hmi::platform::connectivity {
namespace {

constexpr std::size_t kHttpHeaderBytes = 768U;
constexpr std::size_t kSocketBufferBytes =
    hmi::connectivity::kWifiApWebMaximumFrameBytes + 16U;
constexpr char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

void write_http(NetworkClient& client, const char* status,
                const char* content_type, const char* body,
                const std::size_t body_size) {
  char header[192]{};
  const int written = std::snprintf(
      header, sizeof(header),
      "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
      "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
      status, content_type, static_cast<unsigned>(body_size));
  if (written > 0 && static_cast<std::size_t>(written) < sizeof(header)) {
    client.write(reinterpret_cast<const std::uint8_t*>(header),
                 static_cast<std::size_t>(written));
    if (body != nullptr && body_size > 0U) {
      client.write(reinterpret_cast<const std::uint8_t*>(body), body_size);
    }
  }
}

const char* find_header_value(char* request, const char* name) noexcept {
  char* line = std::strstr(request, name);
  if (line == nullptr) {
    return nullptr;
  }
  line += std::strlen(name);
  while (*line == ' ' || *line == '\t') {
    ++line;
  }
  return line;
}

}  // namespace

class EspHostedWifiAp::Impl {
 public:
  ~Impl() { stop(); }

  void attach_web_controller(
      hmi::connectivity::WifiApWebController* controller) noexcept {
    web_controller_ = controller;
  }

  bool start(const hmi::connectivity::WifiApConfig& config) {
    if (state_ != hmi::connectivity::WifiApPortState::off ||
        config.ssid_length == 0U || config.password_length < 8U ||
        !detail::ensure_esp_hosted_pins() || !WiFi.mode(WIFI_AP_STA)) {
      state_ = hmi::connectivity::WifiApPortState::failed;
      return false;
    }
    state_ = hmi::connectivity::WifiApPortState::starting;
    if (!WiFi.softAP(config.ssid, config.password, 1, 0, 2, false,
                     WIFI_AUTH_WPA2_PSK)) {
      WiFi.softAPdisconnect(false);
      WiFi.mode(WIFI_STA);
      state_ = hmi::connectivity::WifiApPortState::failed;
      return false;
    }

    const IPAddress address = WiFi.softAPIP();
    if (static_cast<std::uint32_t>(address) == 0U) {
      WiFi.softAPdisconnect(false);
      WiFi.mode(WIFI_STA);
      state_ = hmi::connectivity::WifiApPortState::failed;
      return false;
    }
    std::snprintf(ipv4_, sizeof(ipv4_), "%u.%u.%u.%u", address[0], address[1],
                  address[2], address[3]);

    server_.reset(new (std::nothrow) NetworkServer(
        80, hmi::connectivity::kWifiApWebMaximumClients + 1U));
    if (!server_) {
      fail_start();
      return false;
    }
    server_->setNoDelay(true);
    server_->begin();
    if (!static_cast<bool>(*server_)) {
      server_.reset();
      fail_start();
      return false;
    }
    next_client_id_ = 1U;
    last_broadcast_revision_ = 0U;
    state_ = hmi::connectivity::WifiApPortState::ready;
    return true;
  }

  void stop() {
    if (state_ == hmi::connectivity::WifiApPortState::off) {
      return;
    }
    state_ = hmi::connectivity::WifiApPortState::stopping;
    for (SocketSlot& slot : slots_) {
      close_slot(slot);
    }
    if (server_) {
      server_->stop();
      server_.reset();
    }
    const bool ap_stopped = WiFi.softAPdisconnect(false);
    const bool station_mode = WiFi.mode(WIFI_STA);
    client_count_ = 0U;
    ipv4_[0] = '\0';
    state_ = ap_stopped && station_mode
                 ? hmi::connectivity::WifiApPortState::off
                 : hmi::connectivity::WifiApPortState::failed;
  }

  void poll() {
    if (state_ != hmi::connectivity::WifiApPortState::ready || !server_) {
      return;
    }
    accept_client();
    for (SocketSlot& slot : slots_) {
      service_slot(slot);
    }
    process_commands();
    client_count_ = WiFi.softAPgetStationNum();
  }

  [[nodiscard]] hmi::connectivity::WifiApPortSnapshot snapshot()
      const noexcept {
    return {state_, client_count_,
            state_ == hmi::connectivity::WifiApPortState::ready &&
                server_ != nullptr && static_cast<bool>(*server_)};
  }

  void copy_ipv4(char* output, const std::size_t capacity) const noexcept {
    if (output == nullptr || capacity == 0U) {
      return;
    }
    std::strncpy(output, ipv4_, capacity - 1U);
    output[capacity - 1U] = '\0';
  }

 private:
  struct SocketSlot {
    NetworkClient client{};
    std::uint32_t id = 0U;
    std::size_t size = 0U;
    bool occupied = false;
    bool websocket = false;
    char buffer[kHttpHeaderBytes + 1U]{};
  };

  void fail_start() {
    WiFi.softAPdisconnect(false);
    WiFi.mode(WIFI_STA);
    ipv4_[0] = '\0';
    state_ = hmi::connectivity::WifiApPortState::failed;
  }

  void accept_client() {
    if (!server_->hasClient()) {
      return;
    }
    NetworkClient client = server_->accept();
    if (!client) {
      return;
    }
    for (SocketSlot& slot : slots_) {
      if (!slot.occupied) {
        slot.client = client;
        slot.id = next_client_id_++;
        if (next_client_id_ == 0U) {
          next_client_id_ = 1U;
        }
        slot.size = 0U;
        slot.occupied = true;
        slot.websocket = false;
        slot.buffer[0] = '\0';
        return;
      }
    }
    constexpr char kBusy[] = "busy\n";
    write_http(client, "503 Service Unavailable", "text/plain", kBusy,
               sizeof(kBusy) - 1U);
    client.stop();
  }

  void close_slot(SocketSlot& slot) {
    if (!slot.occupied) {
      return;
    }
    if (web_controller_ != nullptr && slot.websocket) {
      web_controller_->close_web_client(slot.id);
    }
    slot.client.stop();
    slot.client = NetworkClient{};
    slot.id = 0U;
    slot.size = 0U;
    slot.occupied = false;
    slot.websocket = false;
    slot.buffer[0] = '\0';
  }

  void service_slot(SocketSlot& slot) {
    if (!slot.occupied) {
      return;
    }
    if (!slot.client.connected()) {
      close_slot(slot);
      return;
    }
    const std::size_t limit = slot.websocket ? kSocketBufferBytes
                                             : kHttpHeaderBytes;
    while (slot.client.available() > 0 && slot.size < limit) {
      const int value = slot.client.read();
      if (value < 0) {
        break;
      }
      slot.buffer[slot.size++] = static_cast<char>(value);
    }
    slot.buffer[slot.size] = '\0';
    if (slot.client.available() > 0 && slot.size >= limit) {
      if (!slot.websocket) {
        constexpr char kLarge[] = "request header too large\n";
        write_http(slot.client, "431 Request Header Fields Too Large",
                   "text/plain", kLarge, sizeof(kLarge) - 1U);
      }
      close_slot(slot);
      return;
    }
    if (slot.websocket) {
      service_websocket_frames(slot);
    } else if (std::strstr(slot.buffer, "\r\n\r\n") != nullptr) {
      service_http(slot);
    }
  }

  void service_http(SocketSlot& slot) {
    char method[8]{};
    char path[128]{};
    if (std::sscanf(slot.buffer, "%7s %127s", method, path) != 2) {
      constexpr char kBad[] = "bad request\n";
      write_http(slot.client, "400 Bad Request", "text/plain", kBad,
                 sizeof(kBad) - 1U);
      close_slot(slot);
      return;
    }
    const bool upgrade = std::strstr(slot.buffer, "Upgrade: websocket") != nullptr;
    const auto route = hmi::connectivity::wifi_ap_web_route(method, path, upgrade);
    using hmi::connectivity::WifiApWebHttpRoute;
    switch (route) {
      case WifiApWebHttpRoute::bundle:
        if (web_controller_ == nullptr ||
            web_controller_->web_bundle() == nullptr) {
          constexpr char kUnavailable[] = "web control unavailable\n";
          write_http(slot.client, "503 Service Unavailable", "text/plain",
                     kUnavailable, sizeof(kUnavailable) - 1U);
        } else {
          write_http(slot.client, "200 OK", "text/html; charset=utf-8",
                     web_controller_->web_bundle(),
                     web_controller_->web_bundle_size());
        }
        close_slot(slot);
        return;
      case WifiApWebHttpRoute::favicon:
        write_http(slot.client, "204 No Content", "image/x-icon", nullptr, 0U);
        close_slot(slot);
        return;
      case WifiApWebHttpRoute::health: {
        constexpr char kHealth[] = "ok\n";
        write_http(slot.client, "200 OK", "text/plain", kHealth,
                   sizeof(kHealth) - 1U);
        close_slot(slot);
        return;
      }
      case WifiApWebHttpRoute::websocket:
        if (upgrade_websocket(slot)) {
          return;
        }
        close_slot(slot);
        return;
      case WifiApWebHttpRoute::method_not_allowed: {
        constexpr char kMethod[] = "method not allowed\n";
        write_http(slot.client, "405 Method Not Allowed", "text/plain",
                   kMethod, sizeof(kMethod) - 1U);
        break;
      }
      case WifiApWebHttpRoute::bad_upgrade: {
        constexpr char kUpgrade[] = "websocket upgrade required\n";
        write_http(slot.client, "426 Upgrade Required", "text/plain",
                   kUpgrade, sizeof(kUpgrade) - 1U);
        break;
      }
      case WifiApWebHttpRoute::not_found: {
        constexpr char kMissing[] = "not found\n";
        write_http(slot.client, "404 Not Found", "text/plain", kMissing,
                   sizeof(kMissing) - 1U);
        break;
      }
    }
    close_slot(slot);
  }

  bool upgrade_websocket(SocketSlot& slot) {
    if (web_controller_ == nullptr ||
        !web_controller_->open_web_client(slot.id)) {
      constexpr char kBusy[] = "websocket unavailable\n";
      write_http(slot.client, "503 Service Unavailable", "text/plain", kBusy,
                 sizeof(kBusy) - 1U);
      return false;
    }
    const char* value = find_header_value(slot.buffer, "Sec-WebSocket-Key:");
    if (value == nullptr) {
      web_controller_->close_web_client(slot.id);
      constexpr char kBad[] = "missing websocket key\n";
      write_http(slot.client, "400 Bad Request", "text/plain", kBad,
                 sizeof(kBad) - 1U);
      return false;
    }
    const char* end = std::strstr(value, "\r\n");
    if (end == nullptr || end == value ||
        static_cast<std::size_t>(end - value) > 64U) {
      web_controller_->close_web_client(slot.id);
      return false;
    }
    char joined[128]{};
    const std::size_t key_size = static_cast<std::size_t>(end - value);
    std::memcpy(joined, value, key_size);
    std::memcpy(joined + key_size, kWebSocketGuid, sizeof(kWebSocketGuid));

    SHA1Builder sha1;
    std::uint8_t digest[SHA1_HASH_SIZE]{};
    sha1.begin();
    sha1.add(reinterpret_cast<const std::uint8_t*>(joined),
             key_size + sizeof(kWebSocketGuid) - 1U);
    sha1.calculate();
    sha1.getBytes(digest);
    const String accept = base64::encode(digest, sizeof(digest));
    char response[224]{};
    const int written = std::snprintf(
        response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",
        accept.c_str());
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(response)) {
      web_controller_->close_web_client(slot.id);
      return false;
    }
    slot.client.write(reinterpret_cast<const std::uint8_t*>(response),
                      static_cast<std::size_t>(written));
    slot.websocket = true;
    slot.size = 0U;
    slot.buffer[0] = '\0';
    return true;
  }

  void service_websocket_frames(SocketSlot& slot) {
    while (slot.occupied && slot.size >= 2U) {
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(slot.buffer);
      const bool fin = (bytes[0] & 0x80U) != 0U;
      const std::uint8_t opcode = bytes[0] & 0x0FU;
      const bool masked = (bytes[1] & 0x80U) != 0U;
      std::size_t payload_size = bytes[1] & 0x7FU;
      std::size_t header_size = 2U;
      if (payload_size == 126U) {
        if (slot.size < 4U) {
          return;
        }
        payload_size = (static_cast<std::size_t>(bytes[2]) << 8U) | bytes[3];
        header_size = 4U;
      } else if (payload_size == 127U) {
        close_slot(slot);
        return;
      }
      if (!fin || !masked || payload_size >
                                  hmi::connectivity::kWifiApWebMaximumFrameBytes) {
        close_slot(slot);
        return;
      }
      const std::size_t frame_size = header_size + 4U + payload_size;
      if (slot.size < frame_size) {
        return;
      }
      const std::uint8_t* mask = bytes + header_size;
      const std::uint8_t* payload = mask + 4U;
      char decoded[hmi::connectivity::kWifiApWebMaximumFrameBytes + 1U]{};
      for (std::size_t index = 0U; index < payload_size; ++index) {
        decoded[index] = static_cast<char>(payload[index] ^ mask[index % 4U]);
      }
      consume_frame(slot, frame_size);
      if (opcode == 0x8U) {
        close_slot(slot);
        return;
      }
      if (opcode == 0x9U) {
        send_websocket(slot, decoded, payload_size, 0xAU);
        continue;
      }
      if (opcode != 0x1U || web_controller_ == nullptr) {
        close_slot(slot);
        return;
      }
      const auto status =
          web_controller_->receive_web_frame(slot.id, decoded, payload_size);
      using hmi::connectivity::WifiApWebFrameStatus;
      if (status == WifiApWebFrameStatus::authenticated) {
        char snapshot[hmi::connectivity::kWifiApWebMaximumReplyBytes]{};
        if (web_controller_->copy_web_snapshot_json(snapshot, sizeof(snapshot))) {
          send_websocket(slot, snapshot, std::strlen(snapshot), 0x1U);
        }
      } else if (status != WifiApWebFrameStatus::queued) {
        constexpr char kDenied[] = "{\"type\":\"denied\"}";
        constexpr char kError[] = "{\"type\":\"error\",\"code\":\"rejected\"}";
        const bool denied = status == WifiApWebFrameStatus::denied;
        send_websocket(slot, denied ? kDenied : kError,
                       denied ? sizeof(kDenied) - 1U : sizeof(kError) - 1U,
                       0x1U);
        if (status != WifiApWebFrameStatus::queue_full) {
          close_slot(slot);
          return;
        }
      }
    }
  }

  void consume_frame(SocketSlot& slot, const std::size_t size) noexcept {
    slot.size -= size;
    if (slot.size > 0U) {
      std::memmove(slot.buffer, slot.buffer + size, slot.size);
    }
    slot.buffer[slot.size] = '\0';
  }

  bool send_websocket(SocketSlot& slot, const char* payload,
                      const std::size_t size, const std::uint8_t opcode) {
    if (!slot.occupied || !slot.websocket || payload == nullptr ||
        size > hmi::connectivity::kWifiApWebMaximumReplyBytes) {
      return false;
    }
    std::uint8_t header[4]{};
    std::size_t header_size = 2U;
    header[0] = 0x80U | opcode;
    if (size < 126U) {
      header[1] = static_cast<std::uint8_t>(size);
    } else {
      header[1] = 126U;
      header[2] = static_cast<std::uint8_t>(size >> 8U);
      header[3] = static_cast<std::uint8_t>(size & 0xFFU);
      header_size = 4U;
    }
    return slot.client.write(header, header_size) == header_size &&
           slot.client.write(reinterpret_cast<const std::uint8_t*>(payload),
                             size) == size;
  }

  bool authenticated(const std::uint32_t client_id) const noexcept {
    if (web_controller_ == nullptr) {
      return false;
    }
    std::uint32_t clients[hmi::connectivity::kWifiApWebMaximumClients]{};
    const std::size_t count = web_controller_->copy_authenticated_web_clients(
        clients, hmi::connectivity::kWifiApWebMaximumClients);
    for (std::size_t index = 0U; index < count; ++index) {
      if (clients[index] == client_id) {
        return true;
      }
    }
    return false;
  }

  void broadcast(const char* payload) {
    const std::size_t size = std::strlen(payload);
    for (SocketSlot& slot : slots_) {
      if (slot.websocket && authenticated(slot.id)) {
        send_websocket(slot, payload, size, 0x1U);
      }
    }
  }

  void process_commands() {
    if (web_controller_ == nullptr) {
      return;
    }
    char reply[hmi::connectivity::kWifiApWebMaximumReplyBytes]{};
    std::uint32_t source_client = 0U;
    bool sent = false;
    while (web_controller_->process_web_command(source_client, reply,
                                                sizeof(reply))) {
      broadcast(reply);
      sent = true;
    }
    const std::uint32_t revision = web_controller_->snapshot().revision;
    if (!sent && revision != last_broadcast_revision_ &&
        web_controller_->copy_web_snapshot_json(reply, sizeof(reply))) {
      broadcast(reply);
    }
    last_broadcast_revision_ = revision;
  }

  std::unique_ptr<NetworkServer> server_{};
  hmi::connectivity::WifiApWebController* web_controller_ = nullptr;
  SocketSlot slots_[hmi::connectivity::kWifiApWebMaximumClients]{};
  hmi::connectivity::WifiApPortState state_ =
      hmi::connectivity::WifiApPortState::off;
  char ipv4_[16]{};
  std::uint32_t next_client_id_ = 1U;
  std::uint32_t last_broadcast_revision_ = 0U;
  std::uint8_t client_count_ = 0U;
};

EspHostedWifiAp::EspHostedWifiAp() : impl_(new (std::nothrow) Impl()) {}

EspHostedWifiAp::~EspHostedWifiAp() = default;

bool EspHostedWifiAp::start(const hmi::connectivity::WifiApConfig& config) {
  return impl_ != nullptr && impl_->start(config);
}

void EspHostedWifiAp::request_stop() {
  if (impl_ != nullptr) {
    impl_->stop();
  }
}

void EspHostedWifiAp::poll() {
  if (impl_ != nullptr) {
    impl_->poll();
  }
}

hmi::connectivity::WifiApPortSnapshot EspHostedWifiAp::snapshot()
    const noexcept {
  return impl_ != nullptr ? impl_->snapshot()
                          : hmi::connectivity::WifiApPortSnapshot{
                                hmi::connectivity::WifiApPortState::failed};
}

void EspHostedWifiAp::copy_ipv4(char* output,
                                const std::size_t capacity) const noexcept {
  if (impl_ != nullptr) {
    impl_->copy_ipv4(output, capacity);
  } else if (output != nullptr && capacity != 0U) {
    output[0] = '\0';
  }
}

void EspHostedWifiAp::attach_web_controller(
    hmi::connectivity::WifiApWebController* controller) noexcept {
  if (impl_ != nullptr) {
    impl_->attach_web_controller(controller);
  }
}

bool EspHostedWifiAp::ready() const noexcept { return impl_ != nullptr; }

}  // namespace hmi::platform::connectivity
