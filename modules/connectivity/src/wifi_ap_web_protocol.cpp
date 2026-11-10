#include "hmi/connectivity/wifi_ap_web_protocol.hpp"

#include <cstring>

namespace hmi::connectivity {

WifiApWebHttpRoute wifi_ap_web_route(const char* method, const char* path,
                                     const bool websocket_upgrade) noexcept {
  if (method == nullptr || path == nullptr) {
    return WifiApWebHttpRoute::not_found;
  }
  const bool known = std::strcmp(path, "/") == 0 ||
                     std::strcmp(path, "/favicon.ico") == 0 ||
                     std::strcmp(path, "/healthz") == 0 ||
                     std::strcmp(path, "/ws") == 0;
  if (!known) {
    return WifiApWebHttpRoute::not_found;
  }
  if (std::strcmp(method, "GET") != 0) {
    return WifiApWebHttpRoute::method_not_allowed;
  }
  if (std::strcmp(path, "/") == 0) {
    return WifiApWebHttpRoute::bundle;
  }
  if (std::strcmp(path, "/favicon.ico") == 0) {
    return WifiApWebHttpRoute::favicon;
  }
  if (std::strcmp(path, "/healthz") == 0) {
    return WifiApWebHttpRoute::health;
  }
  return websocket_upgrade ? WifiApWebHttpRoute::websocket
                           : WifiApWebHttpRoute::bad_upgrade;
}

}  // namespace hmi::connectivity
