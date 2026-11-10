#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/connectivity/wifi_ap_runtime.hpp"
#include "hmi/connectivity/wifi_ap_web_types.hpp"

namespace hmi::connectivity {

/**
 * AP lifecycle과 네 switch의 canonical state를 단독 소유하는 controller다.
 *
 * HMI와 web transport는 이 객체의 command/snapshot만 사용한다. runtime은 host가
 * 소유하며 controller보다 오래 살아야 한다. 새 AP session이 시작될 때만 switch와
 * revision을 초기화하고, 같은 session의 client 재접속에는 상태를 유지한다.
 */
class WifiApWebController {
 public:
  explicit WifiApWebController(WifiApRuntime& runtime) noexcept
      : runtime_(runtime) {}
  WifiApWebController(WifiApRuntime& runtime,
                      const WifiApWebSessionSource& session_source) noexcept
      : runtime_(runtime), session_source_(session_source) {}
  ~WifiApWebController();

  WifiApWebController(const WifiApWebController&) = delete;
  WifiApWebController& operator=(const WifiApWebController&) = delete;

  bool start(const char* ssid, const char* password,
             std::uint32_t now_ms) noexcept;
  bool start_session(std::uint32_t now_ms) noexcept;
  bool stop(std::uint32_t now_ms) noexcept;
  void tick(std::uint32_t now_ms) noexcept;
  void shutdown(std::uint32_t now_ms) noexcept;

  [[nodiscard]] WifiApWebSnapshot snapshot() const noexcept;
  [[nodiscard]] WifiApWebCommandResult apply(
      const WifiApWebSwitchCommand& command) noexcept;

  bool set_web_bundle(const char* bundle, std::size_t size) noexcept;
  [[nodiscard]] const char* web_bundle() const noexcept { return web_bundle_; }
  [[nodiscard]] std::size_t web_bundle_size() const noexcept {
    return web_bundle_size_;
  }
  bool begin_web_session(const char* token) noexcept;
  [[nodiscard]] bool web_session_ready() const noexcept;
  void copy_session_token(char* output, std::size_t capacity) const noexcept;
  void copy_ssid(char* output, std::size_t capacity) const noexcept;
  void copy_password(char* output, std::size_t capacity) const noexcept;
  void copy_ipv4(char* output, std::size_t capacity) const noexcept;
  void copy_sta_ipv4(char* output, std::size_t capacity) const noexcept;

  bool open_web_client(std::uint32_t client_id) noexcept;
  void close_web_client(std::uint32_t client_id) noexcept;
  [[nodiscard]] WifiApWebFrameStatus receive_web_frame(
      std::uint32_t client_id, const char* frame, std::size_t size) noexcept;
  bool process_web_command(std::uint32_t& client_id, char* output,
                           std::size_t capacity) noexcept;
  bool copy_web_snapshot_json(char* output, std::size_t capacity,
                              const char* result = nullptr) const noexcept;
  std::size_t copy_authenticated_web_clients(std::uint32_t* output,
                                             std::size_t capacity) const noexcept;

 private:
  struct WebClient {
    std::uint32_t id = 0U;
    bool connected = false;
    bool authenticated = false;
  };

  struct PendingCommand {
    WifiApWebSwitchCommand command{};
    std::uint32_t client_id = 0U;
  };

  void reset_switches() noexcept;
  void clear_web_session() noexcept;
  void update_authenticated_client_count() noexcept;
  [[nodiscard]] WebClient* find_web_client(std::uint32_t client_id) noexcept;
  [[nodiscard]] const WebClient* find_web_client(
      std::uint32_t client_id) const noexcept;

  WifiApRuntime& runtime_;
  WifiApWebSessionSource session_source_{};
  const char* web_bundle_ = nullptr;
  std::size_t web_bundle_size_ = 0U;
  std::uint32_t revision_ = 0U;
  bool switch_values_[kWifiApWebSwitchCount]{};
  char session_token_[kWifiApWebTokenHexBytes + 1U]{};
  WebClient web_clients_[kWifiApWebMaximumClients]{};
  PendingCommand command_queue_[kWifiApWebCommandQueueCapacity]{};
  std::uint8_t queue_head_ = 0U;
  std::uint8_t queue_size_ = 0U;
  std::uint8_t authenticated_client_count_ = 0U;
};

static_assert(sizeof(WifiApWebController) <= 512U,
              "AP web controller must stay inside its embedded budget");

}  // namespace hmi::connectivity
