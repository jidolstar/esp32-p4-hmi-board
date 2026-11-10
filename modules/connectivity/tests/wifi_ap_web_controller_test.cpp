#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "connectivity/virtual_wifi_ap.hpp"
#include "connectivity/virtual_wifi_sta.hpp"
#include "hmi/connectivity/wifi_ap_web_controller.hpp"

namespace {

using hmi::connectivity::WifiApRuntime;
using hmi::connectivity::WifiApRuntimeState;
using hmi::connectivity::WifiApStationPolicy;
using hmi::connectivity::WifiApWebCommandStatus;
using hmi::connectivity::WifiApWebController;
using hmi::connectivity::WifiApWebFrameStatus;
using hmi::connectivity::WifiApWebHttpRoute;
using hmi::connectivity::WifiApWebSessionSource;
using hmi::connectivity::WifiApWebSwitchCommand;
using hmi::connectivity::WifiStaRegistration;
using hmi::platform::connectivity::MemoryWifiStationSettingsStore;
using hmi::platform::connectivity::VirtualWifiAp;
using hmi::platform::connectivity::VirtualWifiSta;

struct Fixture {
  Fixture()
      : station(station_port, store),
        runtime(station, ap_port, WifiApStationPolicy::keep_connected),
        controller(runtime) {
    assert(station.begin(0U, true));
  }

  void step(const unsigned count = 1U) {
    for (unsigned index = 0U; index < count; ++index) {
      now_ms += 100U;
      station.tick(now_ms);
      controller.tick(now_ms);
    }
  }

  void start_ready() {
    assert(controller.start("HMI-Control-A1", "hmi-a1-test", now_ms));
    step(3U);
    assert(controller.snapshot().runtime.state == WifiApRuntimeState::ready);
  }

  VirtualWifiSta station_port;
  MemoryWifiStationSettingsStore store;
  WifiStaRegistration station;
  VirtualWifiAp ap_port;
  WifiApRuntime runtime;
  WifiApWebController controller;
  std::uint32_t now_ms = 0U;
};

WifiApWebSwitchCommand command(const std::uint8_t switch_id, const bool value,
                               const std::uint32_t revision) {
  return WifiApWebSwitchCommand{revision, switch_id, value};
}

WifiApWebFrameStatus frame(WifiApWebController& controller,
                           const std::uint32_t client_id,
                           const char* json) {
  return controller.receive_web_frame(client_id, json, std::strlen(json));
}

bool deterministic_random(void* context, std::uint8_t* output,
                          const std::size_t size) noexcept {
  auto& next = *static_cast<std::uint8_t*>(context);
  for (std::size_t index = 0U; index < size; ++index) {
    output[index] = next++;
  }
  return true;
}

void test_lifecycle_has_one_owner() {
  Fixture fixture;
  assert(fixture.controller.start("HMI-Control-A1", "hmi-a1-test", 0U));
  assert(!fixture.controller.start("duplicate", "duplicate", 0U));
  fixture.step(3U);
  assert(fixture.controller.snapshot().runtime.state ==
         WifiApRuntimeState::ready);
  assert(fixture.controller.stop(fixture.now_ms));
  assert(!fixture.controller.stop(fixture.now_ms));
  fixture.step(3U);
  assert(fixture.controller.snapshot().runtime.state ==
         WifiApRuntimeState::off);
}

void test_command_validation_and_revision() {
  Fixture fixture;

  auto result = fixture.controller.apply(command(1U, true, 0U));
  assert(result.status == WifiApWebCommandStatus::inactive);
  fixture.start_ready();

  result = fixture.controller.apply(command(0U, true, 0U));
  assert(result.status == WifiApWebCommandStatus::invalid_switch);
  result = fixture.controller.apply(command(5U, true, 0U));
  assert(result.status == WifiApWebCommandStatus::invalid_switch);

  result = fixture.controller.apply(command(1U, true, 1U));
  assert(result.status == WifiApWebCommandStatus::stale_revision);
  assert(result.revision == 0U);

  result = fixture.controller.apply(command(1U, true, 0U));
  assert(result.status == WifiApWebCommandStatus::applied);
  assert(result.revision == 1U);
  assert(result.value);
  auto snapshot = fixture.controller.snapshot();
  assert(snapshot.revision == 1U);
  assert(snapshot.switch_values[0]);
  assert(!snapshot.switch_values[1]);

  // 같은 revision과 값은 성공적인 no-op이며 revision을 소비하지 않는다.
  result = fixture.controller.apply(command(1U, true, 1U));
  assert(result.status == WifiApWebCommandStatus::unchanged);
  assert(result.revision == 1U);

  // 이미 적용된 명령을 예전 revision으로 재전송하면 최신 snapshot을 요구한다.
  result = fixture.controller.apply(command(1U, true, 0U));
  assert(result.status == WifiApWebCommandStatus::stale_revision);
  assert(result.revision == 1U);
  assert(result.value);

  result = fixture.controller.apply(command(4U, true, 1U));
  assert(result.status == WifiApWebCommandStatus::applied);
  snapshot = fixture.controller.snapshot();
  assert(snapshot.revision == 2U);
  assert(snapshot.switch_values[0]);
  assert(snapshot.switch_values[3]);
}

void test_new_session_reset_and_secret_clear() {
  Fixture fixture;
  fixture.start_ready();
  assert(fixture.controller.apply(command(2U, true, 0U)).status ==
         WifiApWebCommandStatus::applied);
  assert(fixture.controller.stop(fixture.now_ms));
  fixture.step(3U);

  auto snapshot = fixture.controller.snapshot();
  assert(snapshot.revision == 1U);
  assert(snapshot.switch_values[1]);

  char password[64]{};
  fixture.runtime.copy_password(password, sizeof(password));
  assert(password[0] == '\0');

  // 거부된 시작은 아직 끝난 session의 canonical state를 변경하지 않는다.
  assert(!fixture.controller.start("", "short", fixture.now_ms));
  snapshot = fixture.controller.snapshot();
  assert(snapshot.revision == 1U);
  assert(snapshot.switch_values[1]);

  assert(fixture.controller.start("HMI-Control-A1", "next-session",
                                  fixture.now_ms));
  snapshot = fixture.controller.snapshot();
  assert(snapshot.revision == 0U);
  for (const bool value : snapshot.switch_values) {
    assert(!value);
  }

  fixture.controller.shutdown(fixture.now_ms);
  std::memset(password, 'x', sizeof(password));
  fixture.runtime.copy_password(password, sizeof(password));
  assert(password[0] == '\0');
  snapshot = fixture.controller.snapshot();
  assert(snapshot.runtime.state == WifiApRuntimeState::off);
  assert(snapshot.revision == 0U);
}

void test_fixed_http_routes() {
  using hmi::connectivity::wifi_ap_web_route;
  assert(wifi_ap_web_route("GET", "/", false) == WifiApWebHttpRoute::bundle);
  assert(wifi_ap_web_route("GET", "/favicon.ico", false) ==
         WifiApWebHttpRoute::favicon);
  assert(wifi_ap_web_route("GET", "/healthz", false) ==
         WifiApWebHttpRoute::health);
  assert(wifi_ap_web_route("GET", "/ws", true) ==
         WifiApWebHttpRoute::websocket);
  assert(wifi_ap_web_route("GET", "/ws", false) ==
         WifiApWebHttpRoute::bad_upgrade);
  assert(wifi_ap_web_route("POST", "/", false) ==
         WifiApWebHttpRoute::method_not_allowed);
  assert(wifi_ap_web_route("GET", "/unknown", false) ==
         WifiApWebHttpRoute::not_found);
}

void test_generated_session_and_secret_boundary() {
  VirtualWifiSta station_port;
  MemoryWifiStationSettingsStore store;
  WifiStaRegistration station(station_port, store);
  VirtualWifiAp ap_port;
  WifiApRuntime runtime(station, ap_port, WifiApStationPolicy::keep_connected);
  std::uint8_t random_seed = 0U;
  WifiApWebSessionSource source{};
  source.random = {&random_seed, &deterministic_random};
  std::memcpy(source.device_suffix, "BEEF", 5U);
  WifiApWebController controller(runtime, source);
  assert(station.begin(0U, true));
  assert(controller.set_web_bundle("<html></html>", 13U));
  assert(controller.start_session(0U));
  for (std::uint32_t now_ms = 100U; now_ms <= 300U; now_ms += 100U) {
    station.tick(now_ms);
    controller.tick(now_ms);
  }
  assert(controller.web_session_ready());

  char ssid[40]{};
  char password[40]{};
  char token[40]{};
  controller.copy_ssid(ssid, sizeof(ssid));
  controller.copy_password(password, sizeof(password));
  controller.copy_session_token(token, sizeof(token));
  assert(std::strcmp(ssid, "HMI-Control-BEEF") == 0);
  assert(std::strlen(password) == 12U);
  assert(std::strcmp(token, "0C0D0E0F101112131415161718191A1B") == 0);

  char snapshot[256]{};
  assert(controller.copy_web_snapshot_json(snapshot, sizeof(snapshot)));
  assert(std::strstr(snapshot, password) == nullptr);
  assert(std::strstr(snapshot, token) == nullptr);
  controller.shutdown(400U);
  std::memset(token, 'x', sizeof(token));
  controller.copy_session_token(token, sizeof(token));
  assert(token[0] == '\0');
}

void test_authentication_validation_queue_and_reconnect() {
  Fixture fixture;
  fixture.start_ready();
  constexpr char kToken[] = "0123456789ABCDEF0123456789ABCDEF";
  assert(fixture.controller.set_web_bundle("x", 1U));
  assert(fixture.controller.begin_web_session(kToken));
  assert(fixture.controller.open_web_client(11U));
  assert(fixture.controller.open_web_client(12U));
  assert(!fixture.controller.open_web_client(13U));
  assert(frame(fixture.controller, 11U,
               "{\"token\":\"BADBADBADBADBADBADBADBADBADBADBA\"}") ==
         WifiApWebFrameStatus::denied);
  assert(frame(fixture.controller, 11U,
               "{\"token\":\"0123456789ABCDEF0123456789ABCDEF\"}") ==
         WifiApWebFrameStatus::authenticated);
  assert(fixture.controller.snapshot().authenticated_client_count == 1U);

  assert(frame(fixture.controller, 11U,
               "{\"id\":1,\"value\":true,\"revision\":0,\"extra\":1}") ==
         WifiApWebFrameStatus::malformed);
  assert(frame(fixture.controller, 11U,
               "{\"id\":1,\"id\":1,\"revision\":0}") ==
         WifiApWebFrameStatus::malformed);
  assert(frame(fixture.controller, 11U,
               "{\"id\":1,\"value\":true,\"revision\":0}") ==
         WifiApWebFrameStatus::queued);

  std::uint32_t client_id = 0U;
  char reply[256]{};
  assert(fixture.controller.process_web_command(client_id, reply,
                                                sizeof(reply)));
  assert(client_id == 11U);
  assert(std::strstr(reply, "\"result\":\"applied\"") != nullptr);
  assert(std::strstr(reply, "\"switches\":[true,false,false,false]") !=
         nullptr);

  fixture.controller.close_web_client(11U);
  assert(fixture.controller.snapshot().authenticated_client_count == 0U);
  assert(fixture.controller.open_web_client(13U));
  assert(frame(fixture.controller, 13U,
               "{\"token\":\"0123456789ABCDEF0123456789ABCDEF\"}") ==
         WifiApWebFrameStatus::authenticated);
  std::memset(reply, 0, sizeof(reply));
  assert(fixture.controller.copy_web_snapshot_json(reply, sizeof(reply)));
  assert(std::strstr(reply, "\"revision\":1") != nullptr);
  assert(std::strstr(reply, "\"switches\":[true,false,false,false]") !=
         nullptr);

  constexpr char kNoOp[] =
      "{\"id\":1,\"value\":true,\"revision\":1}";
  for (std::size_t index = 0U;
       index < hmi::connectivity::kWifiApWebCommandQueueCapacity; ++index) {
    assert(fixture.controller.receive_web_frame(13U, kNoOp,
                                                sizeof(kNoOp) - 1U) ==
           WifiApWebFrameStatus::queued);
  }
  assert(fixture.controller.receive_web_frame(13U, kNoOp,
                                              sizeof(kNoOp) - 1U) ==
         WifiApWebFrameStatus::queue_full);

  char oversized[hmi::connectivity::kWifiApWebMaximumFrameBytes + 2U]{};
  std::memset(oversized, 'x', sizeof(oversized));
  assert(fixture.controller.receive_web_frame(13U, oversized,
                                              sizeof(oversized)) ==
         WifiApWebFrameStatus::frame_too_large);
}

}  // namespace

int main() {
  std::fprintf(stderr, "wifi_ap_web_controller: lifecycle\n");
  test_lifecycle_has_one_owner();
  std::fprintf(stderr, "wifi_ap_web_controller: command-revision\n");
  test_command_validation_and_revision();
  std::fprintf(stderr, "wifi_ap_web_controller: session-secret\n");
  test_new_session_reset_and_secret_clear();
  std::fprintf(stderr, "wifi_ap_web_controller: routes\n");
  test_fixed_http_routes();
  std::fprintf(stderr, "wifi_ap_web_controller: generated-session\n");
  test_generated_session_and_secret_boundary();
  std::fprintf(stderr, "wifi_ap_web_controller: protocol\n");
  test_authentication_validation_queue_and_reconnect();
  std::fprintf(stderr, "wifi_ap_web_controller: ok\n");
  return 0;
}
