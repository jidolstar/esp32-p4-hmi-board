#pragma once

#include <cstdint>

#include "hmi/connectivity/connectivity_types.hpp"

namespace hmi::connectivity {

enum class WifiStationSettingsLoadStatus : std::uint8_t {
  loaded = 0,
  not_found,
  invalid,
  storage_error,
};

/** registration core가 Preferences/NVS API를 알지 않도록 하는 credential 저장 경계다. */
class WifiStationSettingsStore {
 public:
  virtual ~WifiStationSettingsStore() = default;
  virtual WifiStationSettingsLoadStatus load(
      WifiStationCredential& output) = 0;
  virtual bool save(const WifiStationCredential& credential) = 0;
  virtual bool erase() = 0;
};

}  // namespace hmi::connectivity
