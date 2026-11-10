#include "hmi/connectivity/wifi_ap_web_controller.hpp"

#include <cstdio>
#include <cstring>
#include <limits>

namespace hmi::connectivity {
namespace {

constexpr char kPasswordAlphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
constexpr std::size_t kGeneratedPasswordBytes = 12U;

void secure_clear(void* data, const std::size_t size) noexcept {
  volatile auto* bytes = static_cast<volatile std::uint8_t*>(data);
  for (std::size_t index = 0U; index < size; ++index) {
    bytes[index] = 0U;
  }
}

void copy_text(const char* source, char* output,
               const std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0U) {
    return;
  }
  std::strncpy(output, source == nullptr ? "" : source, capacity - 1U);
  output[capacity - 1U] = '\0';
}

bool is_hex(const char value) noexcept {
  return (value >= '0' && value <= '9') ||
         (value >= 'A' && value <= 'F') ||
         (value >= 'a' && value <= 'f');
}

bool valid_suffix(const char suffix[5]) noexcept {
  return suffix != nullptr && is_hex(suffix[0]) && is_hex(suffix[1]) &&
         is_hex(suffix[2]) && is_hex(suffix[3]) && suffix[4] == '\0';
}

bool valid_token(const char* token) noexcept {
  if (token == nullptr) {
    return false;
  }
  for (std::size_t index = 0U; index < kWifiApWebTokenHexBytes; ++index) {
    if (!is_hex(token[index])) {
      return false;
    }
  }
  return token[kWifiApWebTokenHexBytes] == '\0';
}

bool constant_time_equal(const char* left, const char* right,
                         const std::size_t size) noexcept {
  std::uint8_t difference = 0U;
  for (std::size_t index = 0U; index < size; ++index) {
    difference |= static_cast<std::uint8_t>(left[index]) ^
                  static_cast<std::uint8_t>(right[index]);
  }
  return difference == 0U;
}

struct JsonCursor {
  const char* current = nullptr;
  const char* end = nullptr;
};

void skip_space(JsonCursor& cursor) noexcept {
  while (cursor.current < cursor.end &&
         (*cursor.current == ' ' || *cursor.current == '\t' ||
          *cursor.current == '\r' || *cursor.current == '\n')) {
    ++cursor.current;
  }
}

bool consume(JsonCursor& cursor, const char expected) noexcept {
  skip_space(cursor);
  if (cursor.current >= cursor.end || *cursor.current != expected) {
    return false;
  }
  ++cursor.current;
  return true;
}

bool parse_string(JsonCursor& cursor, char* output,
                  const std::size_t capacity) noexcept {
  if (!consume(cursor, '"') || output == nullptr || capacity == 0U) {
    return false;
  }
  std::size_t size = 0U;
  while (cursor.current < cursor.end && *cursor.current != '"') {
    const unsigned char value = static_cast<unsigned char>(*cursor.current);
    // Protocol key와 token은 escape나 제어문자가 필요 없는 ASCII만 허용한다.
    if (value < 0x20U || value > 0x7EU || value == '\\' ||
        size + 1U >= capacity) {
      return false;
    }
    output[size++] = *cursor.current++;
  }
  if (cursor.current >= cursor.end || *cursor.current != '"') {
    return false;
  }
  ++cursor.current;
  output[size] = '\0';
  return true;
}

bool parse_uint32(JsonCursor& cursor, std::uint32_t& output) noexcept {
  skip_space(cursor);
  if (cursor.current >= cursor.end || *cursor.current < '0' ||
      *cursor.current > '9') {
    return false;
  }
  std::uint32_t value = 0U;
  do {
    const std::uint32_t digit =
        static_cast<std::uint32_t>(*cursor.current - '0');
    if (value > (std::numeric_limits<std::uint32_t>::max() - digit) / 10U) {
      return false;
    }
    value = value * 10U + digit;
    ++cursor.current;
  } while (cursor.current < cursor.end && *cursor.current >= '0' &&
           *cursor.current <= '9');
  output = value;
  return true;
}

bool parse_bool(JsonCursor& cursor, bool& output) noexcept {
  skip_space(cursor);
  const std::size_t remaining =
      static_cast<std::size_t>(cursor.end - cursor.current);
  if (remaining >= 4U && std::memcmp(cursor.current, "true", 4U) == 0) {
    cursor.current += 4U;
    output = true;
    return true;
  }
  if (remaining >= 5U && std::memcmp(cursor.current, "false", 5U) == 0) {
    cursor.current += 5U;
    output = false;
    return true;
  }
  return false;
}

bool finish_object(JsonCursor& cursor) noexcept {
  if (!consume(cursor, '}')) {
    return false;
  }
  skip_space(cursor);
  return cursor.current == cursor.end;
}

bool parse_auth_frame(const char* frame, const std::size_t size,
                      char token[kWifiApWebTokenHexBytes + 1U]) noexcept {
  JsonCursor cursor{frame, frame + size};
  if (!consume(cursor, '{')) {
    return false;
  }
  char key[9]{};
  if (!parse_string(cursor, key, sizeof(key)) ||
      std::strcmp(key, "token") != 0 || !consume(cursor, ':') ||
      !parse_string(cursor, token, kWifiApWebTokenHexBytes + 1U) ||
      !valid_token(token)) {
    return false;
  }
  return finish_object(cursor);
}

bool parse_command_frame(const char* frame, const std::size_t size,
                         WifiApWebSwitchCommand& command) noexcept {
  JsonCursor cursor{frame, frame + size};
  if (!consume(cursor, '{')) {
    return false;
  }

  bool has_id = false;
  bool has_value = false;
  bool has_revision = false;
  for (std::size_t field = 0U; field < 3U; ++field) {
    char key[17]{};
    if (!parse_string(cursor, key, sizeof(key)) || !consume(cursor, ':')) {
      return false;
    }
    if (std::strcmp(key, "id") == 0) {
      std::uint32_t id = 0U;
      if (has_id || !parse_uint32(cursor, id) || id > 255U) {
        return false;
      }
      command.switch_id = static_cast<std::uint8_t>(id);
      has_id = true;
    } else if (std::strcmp(key, "value") == 0) {
      if (has_value || !parse_bool(cursor, command.value)) {
        return false;
      }
      has_value = true;
    } else if (std::strcmp(key, "revision") == 0) {
      if (has_revision ||
          !parse_uint32(cursor, command.expected_revision)) {
        return false;
      }
      has_revision = true;
    } else {
      return false;
    }

    skip_space(cursor);
    if (field < 2U && !consume(cursor, ',')) {
      return false;
    }
  }
  return has_id && has_value && has_revision && finish_object(cursor);
}

const char* command_result_name(const WifiApWebCommandStatus status) noexcept {
  switch (status) {
    case WifiApWebCommandStatus::applied:
      return "applied";
    case WifiApWebCommandStatus::unchanged:
      return "unchanged";
    case WifiApWebCommandStatus::invalid_switch:
      return "invalid_switch";
    case WifiApWebCommandStatus::inactive:
      return "inactive";
    case WifiApWebCommandStatus::stale_revision:
      return "stale_revision";
    case WifiApWebCommandStatus::revision_exhausted:
      return "revision_exhausted";
  }
  return "inactive";
}

}  // namespace

WifiApWebController::~WifiApWebController() {
  clear_web_session();
}

bool WifiApWebController::start(const char* ssid, const char* password,
                                const std::uint32_t now_ms) noexcept {
  if (!runtime_.start(ssid, password, now_ms)) {
    return false;
  }
  // 완전히 새 AP session을 받아들인 시점에만 canonical state를 초기화한다.
  reset_switches();
  clear_web_session();
  return true;
}

bool WifiApWebController::start_session(const std::uint32_t now_ms) noexcept {
  if (!session_source_.random.valid() ||
      !valid_suffix(session_source_.device_suffix)) {
    return false;
  }

  std::uint8_t random_bytes[kGeneratedPasswordBytes + 16U]{};
  char password[kGeneratedPasswordBytes + 1U]{};
  char token[kWifiApWebTokenHexBytes + 1U]{};
  char ssid[32]{};
  const bool filled = session_source_.random.fill(
      session_source_.random.context, random_bytes, sizeof(random_bytes));
  if (!filled) {
    secure_clear(random_bytes, sizeof(random_bytes));
    return false;
  }

  constexpr std::size_t alphabet_size = sizeof(kPasswordAlphabet) - 1U;
  for (std::size_t index = 0U; index < kGeneratedPasswordBytes; ++index) {
    password[index] = kPasswordAlphabet[random_bytes[index] % alphabet_size];
  }
  constexpr char kHex[] = "0123456789ABCDEF";
  for (std::size_t index = 0U; index < 16U; ++index) {
    const std::uint8_t value = random_bytes[kGeneratedPasswordBytes + index];
    token[index * 2U] = kHex[value >> 4U];
    token[index * 2U + 1U] = kHex[value & 0x0FU];
  }
  std::snprintf(ssid, sizeof(ssid), "HMI-Control-%s",
                session_source_.device_suffix);

  const bool started = start(ssid, password, now_ms);
  const bool session_ready = started && begin_web_session(token);
  if (started && !session_ready) {
    runtime_.shutdown(now_ms);
    reset_switches();
  }
  secure_clear(password, sizeof(password));
  secure_clear(token, sizeof(token));
  secure_clear(random_bytes, sizeof(random_bytes));
  return session_ready;
}

bool WifiApWebController::stop(const std::uint32_t now_ms) noexcept {
  return runtime_.stop(now_ms);
}

void WifiApWebController::tick(const std::uint32_t now_ms) noexcept {
  runtime_.tick(now_ms);
  if (runtime_.snapshot().state == WifiApRuntimeState::off &&
      session_token_[0] != '\0') {
    clear_web_session();
  }
}

void WifiApWebController::shutdown(const std::uint32_t now_ms) noexcept {
  runtime_.shutdown(now_ms);
  reset_switches();
  clear_web_session();
}

WifiApWebSnapshot WifiApWebController::snapshot() const noexcept {
  WifiApWebSnapshot result{};
  result.revision = revision_;
  result.runtime = runtime_.snapshot();
  for (std::size_t index = 0U; index < kWifiApWebSwitchCount; ++index) {
    result.switch_values[index] = switch_values_[index];
  }
  result.authenticated_client_count = authenticated_client_count_;
  return result;
}

WifiApWebCommandResult WifiApWebController::apply(
    const WifiApWebSwitchCommand& command) noexcept {
  WifiApWebCommandResult result{};
  result.revision = revision_;
  result.switch_id = command.switch_id;

  if (command.switch_id < static_cast<std::uint8_t>(WifiApWebSwitchId::one) ||
      command.switch_id > static_cast<std::uint8_t>(WifiApWebSwitchId::four)) {
    result.status = WifiApWebCommandStatus::invalid_switch;
    return result;
  }

  const std::size_t index = static_cast<std::size_t>(command.switch_id - 1U);
  result.value = switch_values_[index];
  if (runtime_.snapshot().state != WifiApRuntimeState::ready) {
    result.status = WifiApWebCommandStatus::inactive;
    return result;
  }
  if (command.expected_revision != revision_) {
    result.status = WifiApWebCommandStatus::stale_revision;
    return result;
  }
  if (command.value == switch_values_[index]) {
    result.status = WifiApWebCommandStatus::unchanged;
    return result;
  }
  if (revision_ == std::numeric_limits<std::uint32_t>::max()) {
    result.status = WifiApWebCommandStatus::revision_exhausted;
    return result;
  }

  switch_values_[index] = command.value;
  ++revision_;
  result.status = WifiApWebCommandStatus::applied;
  result.revision = revision_;
  result.value = command.value;
  return result;
}

bool WifiApWebController::set_web_bundle(const char* bundle,
                                         const std::size_t size) noexcept {
  if (bundle == nullptr || size == 0U || size > 24U * 1024U) {
    return false;
  }
  web_bundle_ = bundle;
  web_bundle_size_ = size;
  return true;
}

bool WifiApWebController::begin_web_session(const char* token) noexcept {
  if (!valid_token(token)) {
    return false;
  }
  clear_web_session();
  std::memcpy(session_token_, token, kWifiApWebTokenHexBytes);
  session_token_[kWifiApWebTokenHexBytes] = '\0';
  return true;
}

bool WifiApWebController::web_session_ready() const noexcept {
  return web_bundle_ != nullptr && web_bundle_size_ > 0U &&
         session_token_[0] != '\0' &&
         runtime_.snapshot().state == WifiApRuntimeState::ready;
}

void WifiApWebController::copy_session_token(
    char* output, const std::size_t capacity) const noexcept {
  copy_text(session_token_, output, capacity);
}

void WifiApWebController::copy_ssid(char* output,
                                    const std::size_t capacity) const noexcept {
  runtime_.copy_ssid(output, capacity);
}

void WifiApWebController::copy_password(
    char* output, const std::size_t capacity) const noexcept {
  runtime_.copy_password(output, capacity);
}

void WifiApWebController::copy_ipv4(char* output,
                                    const std::size_t capacity) const noexcept {
  runtime_.copy_ipv4(output, capacity);
}

void WifiApWebController::copy_sta_ipv4(
    char* output, const std::size_t capacity) const noexcept {
  runtime_.copy_sta_ipv4(output, capacity);
}

bool WifiApWebController::open_web_client(
    const std::uint32_t client_id) noexcept {
  if (client_id == 0U || session_token_[0] == '\0' ||
      runtime_.snapshot().state != WifiApRuntimeState::ready) {
    return false;
  }
  if (find_web_client(client_id) != nullptr) {
    return true;
  }
  for (WebClient& client : web_clients_) {
    if (!client.connected) {
      client = {client_id, true, false};
      return true;
    }
  }
  return false;
}

void WifiApWebController::close_web_client(
    const std::uint32_t client_id) noexcept {
  WebClient* client = find_web_client(client_id);
  if (client != nullptr) {
    *client = {};
    update_authenticated_client_count();
  }
}

WifiApWebFrameStatus WifiApWebController::receive_web_frame(
    const std::uint32_t client_id, const char* frame,
    const std::size_t size) noexcept {
  WebClient* client = find_web_client(client_id);
  if (client == nullptr) {
    return WifiApWebFrameStatus::unknown_client;
  }
  if (runtime_.snapshot().state != WifiApRuntimeState::ready ||
      session_token_[0] == '\0') {
    return WifiApWebFrameStatus::inactive;
  }
  if (frame == nullptr || size == 0U) {
    return WifiApWebFrameStatus::malformed;
  }
  if (size > kWifiApWebMaximumFrameBytes) {
    return WifiApWebFrameStatus::frame_too_large;
  }

  if (!client->authenticated) {
    char candidate[kWifiApWebTokenHexBytes + 1U]{};
    const bool parsed = parse_auth_frame(frame, size, candidate);
    const bool matches = parsed && constant_time_equal(
                                      candidate, session_token_,
                                      kWifiApWebTokenHexBytes);
    secure_clear(candidate, sizeof(candidate));
    if (!matches) {
      return WifiApWebFrameStatus::denied;
    }
    client->authenticated = true;
    update_authenticated_client_count();
    return WifiApWebFrameStatus::authenticated;
  }

  WifiApWebSwitchCommand command{};
  if (!parse_command_frame(frame, size, command)) {
    return WifiApWebFrameStatus::malformed;
  }
  if (queue_size_ >= kWifiApWebCommandQueueCapacity) {
    return WifiApWebFrameStatus::queue_full;
  }
  const std::size_t tail =
      (static_cast<std::size_t>(queue_head_) + queue_size_) %
      kWifiApWebCommandQueueCapacity;
  command_queue_[tail] = {command, client_id};
  ++queue_size_;
  return WifiApWebFrameStatus::queued;
}

bool WifiApWebController::process_web_command(
    std::uint32_t& client_id, char* output,
    const std::size_t capacity) noexcept {
  while (queue_size_ > 0U) {
    const PendingCommand pending = command_queue_[queue_head_];
    command_queue_[queue_head_] = {};
    queue_head_ = static_cast<std::uint8_t>(
        (static_cast<std::size_t>(queue_head_) + 1U) %
        kWifiApWebCommandQueueCapacity);
    --queue_size_;
    const WebClient* client = find_web_client(pending.client_id);
    if (client == nullptr || !client->authenticated) {
      continue;
    }
    const WifiApWebCommandResult result = apply(pending.command);
    client_id = pending.client_id;
    return copy_web_snapshot_json(output, capacity,
                                  command_result_name(result.status));
  }
  return false;
}

bool WifiApWebController::copy_web_snapshot_json(
    char* output, const std::size_t capacity, const char* result) const noexcept {
  if (output == nullptr || capacity == 0U) {
    return false;
  }
  const int written = result == nullptr
                          ? std::snprintf(
                                output, capacity,
                                "{\"type\":\"snapshot\",\"revision\":%lu,"
                                "\"switches\":[%s,%s,%s,%s],\"clients\":%u}",
                                static_cast<unsigned long>(revision_),
                                switch_values_[0] ? "true" : "false",
                                switch_values_[1] ? "true" : "false",
                                switch_values_[2] ? "true" : "false",
                                switch_values_[3] ? "true" : "false",
                                authenticated_client_count_)
                          : std::snprintf(
                                output, capacity,
                                "{\"type\":\"snapshot\",\"revision\":%lu,"
                                "\"switches\":[%s,%s,%s,%s],\"clients\":%u,"
                                "\"result\":\"%s\"}",
                                static_cast<unsigned long>(revision_),
                                switch_values_[0] ? "true" : "false",
                                switch_values_[1] ? "true" : "false",
                                switch_values_[2] ? "true" : "false",
                                switch_values_[3] ? "true" : "false",
                                authenticated_client_count_, result);
  if (written < 0 || static_cast<std::size_t>(written) >= capacity) {
    output[0] = '\0';
    return false;
  }
  return true;
}

std::size_t WifiApWebController::copy_authenticated_web_clients(
    std::uint32_t* output, const std::size_t capacity) const noexcept {
  if (output == nullptr || capacity == 0U) {
    return 0U;
  }
  std::size_t copied = 0U;
  for (const WebClient& client : web_clients_) {
    if (client.connected && client.authenticated && copied < capacity) {
      output[copied++] = client.id;
    }
  }
  return copied;
}

void WifiApWebController::reset_switches() noexcept {
  revision_ = 0U;
  for (bool& value : switch_values_) {
    value = false;
  }
}

void WifiApWebController::clear_web_session() noexcept {
  secure_clear(session_token_, sizeof(session_token_));
  for (WebClient& client : web_clients_) {
    client = {};
  }
  for (PendingCommand& command : command_queue_) {
    command = {};
  }
  queue_head_ = 0U;
  queue_size_ = 0U;
  authenticated_client_count_ = 0U;
}

void WifiApWebController::update_authenticated_client_count() noexcept {
  authenticated_client_count_ = 0U;
  for (const WebClient& client : web_clients_) {
    if (client.connected && client.authenticated) {
      ++authenticated_client_count_;
    }
  }
}

WifiApWebController::WebClient* WifiApWebController::find_web_client(
    const std::uint32_t client_id) noexcept {
  for (WebClient& client : web_clients_) {
    if (client.connected && client.id == client_id) {
      return &client;
    }
  }
  return nullptr;
}

const WifiApWebController::WebClient* WifiApWebController::find_web_client(
    const std::uint32_t client_id) const noexcept {
  for (const WebClient& client : web_clients_) {
    if (client.connected && client.id == client_id) {
      return &client;
    }
  }
  return nullptr;
}

}  // namespace hmi::connectivity
