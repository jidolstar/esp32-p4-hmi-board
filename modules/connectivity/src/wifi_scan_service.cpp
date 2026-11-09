#include "hmi/connectivity/wifi_scan_service.hpp"

#include <algorithm>
#include <cstring>

namespace hmi::connectivity {
namespace {

bool same_network(const WifiScanEntry& left,
                  const WifiScanEntry& right) noexcept {
  return left.ssid_length == right.ssid_length &&
         left.security == right.security &&
         std::memcmp(left.ssid, right.ssid, left.ssid_length) == 0;
}

}  // namespace

bool WifiScanService::request() noexcept {
  if (busy()) {
    return false;
  }
  entries_.reset();
  ++snapshot_.generation;
  snapshot_.maximum = static_cast<std::uint8_t>(adapter_.scan_limit());
  snapshot_.attempt = 0U;
  snapshot_.state = WifiScanState::queued;
  snapshot_.count = 0U;
  snapshot_.truncated = false;
  return true;
}

bool WifiScanService::busy() const noexcept {
  return active_ || snapshot_.state == WifiScanState::queued;
}

void WifiScanService::cancel() noexcept {
  if (active_) {
    adapter_.cancel_scan();
  }
  ++snapshot_.generation;
  entries_.reset();
  snapshot_.count = 0U;
  snapshot_.state = WifiScanState::cancelled;
  snapshot_.draining = active_;
}

void WifiScanService::tick(const std::uint32_t now_ms,
                           const bool allowed) noexcept {
  if (snapshot_.state == WifiScanState::queued && allowed) {
    if (!adapter_.begin_scan(snapshot_.generation)) {
      snapshot_.state = WifiScanState::failed;
      return;
    }
    active_ = true;
    started_ms_ = now_ms;
    snapshot_.state = WifiScanState::scanning;
  }
  if (!active_) {
    return;
  }

  snapshot_.attempt = static_cast<std::uint8_t>(adapter_.scan_attempt());
  if (snapshot_.state == WifiScanState::scanning && !allowed) {
    cancel();
  }
  const std::uint32_t timeout_ms =
      snapshot_.maximum >= 3U ? 30000U : 15000U;
  if (snapshot_.state == WifiScanState::scanning &&
      now_ms - started_ms_ >= timeout_ms) {
    cancel();
    snapshot_.state = WifiScanState::timed_out;
  }
  if (adapter_.scan_running()) {
    return;
  }

  WifiScanBatch result = adapter_.take_scan_result();
  active_ = false;
  snapshot_.draining = false;
  if (snapshot_.state != WifiScanState::scanning) {
    return;
  }
  if (result.generation != snapshot_.generation || !result.succeeded ||
      result.count > kMaximumWifiScanEntries ||
      (result.count != 0U && !result.entries)) {
    snapshot_.state = WifiScanState::failed;
    return;
  }

  std::size_t visible_count = 0U;
  for (std::size_t source = 0U; source < result.count; ++source) {
    const WifiScanEntry& candidate = result.entries[source];
    if (candidate.ssid_length > kMaximumSsidBytes ||
        candidate.ssid[candidate.ssid_length] != '\0') {
      snapshot_.state = WifiScanState::failed;
      return;
    }
    if (candidate.ssid_length == 0U) {
      continue;
    }

    std::size_t duplicate = visible_count;
    for (std::size_t index = 0U; index < visible_count; ++index) {
      if (same_network(result.entries[index], candidate)) {
        duplicate = index;
        break;
      }
    }
    if (duplicate != visible_count) {
      if (candidate.rssi > result.entries[duplicate].rssi) {
        result.entries[duplicate] = candidate;
      }
      continue;
    }
    if (visible_count != source) {
      result.entries[visible_count] = candidate;
    }
    ++visible_count;
  }

  if (visible_count > 1U) {
    std::sort(result.entries.get(), result.entries.get() + visible_count,
              [](const WifiScanEntry& left, const WifiScanEntry& right) {
                return left.rssi > right.rssi;
              });
  }
  entries_ = std::move(result.entries);
  snapshot_.count = static_cast<std::uint8_t>(visible_count);
  snapshot_.truncated = result.truncated;
  snapshot_.state = WifiScanState::ready;
}

const WifiScanEntry* WifiScanService::entry(
    const std::uint32_t generation,
    const std::size_t index) const noexcept {
  if (generation != snapshot_.generation ||
      snapshot_.state != WifiScanState::ready || index >= snapshot_.count) {
    return nullptr;
  }
  return &entries_[index];
}

}  // namespace hmi::connectivity
