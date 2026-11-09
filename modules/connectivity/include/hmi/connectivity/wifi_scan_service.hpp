#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "hmi/connectivity/connectivity_types.hpp"

namespace hmi::connectivity {

inline constexpr std::size_t kMaximumWifiScanEntries = 32U;

enum class WifiSecurity : std::uint8_t {
  open,
  personal,
  wpa3_personal,
  unsupported,
};

/** 표시 변환 전 SSID byte와 AP identity다. driver pointer와 password를 소유하지 않는다. */
struct WifiScanEntry {
  char ssid[kMaximumSsidBytes + 1U]{};
  std::uint8_t ssid_length = 0U;
  std::uint8_t bssid[6]{};
  std::int16_t rssi = 0;
  std::uint8_t channel = 0U;
  WifiSecurity security = WifiSecurity::unsupported;
};

/** 실제 결과 수만 checked heap에 만들고 adapter에서 service로 한 번 move한다. */
struct WifiScanBatch {
  std::unique_ptr<WifiScanEntry[]> entries{};
  std::uint32_t generation = 0U;
  std::uint16_t count = 0U;
  bool truncated = false;
  bool succeeded = false;
};

/** driver scan은 adapter worker가 수행하며 cancel 뒤 완료 결과까지 drain한다. */
class WifiScanAdapter {
 public:
  virtual ~WifiScanAdapter() = default;
  virtual bool begin_scan(std::uint32_t generation) = 0;
  [[nodiscard]] virtual bool scan_running() const = 0;
  virtual WifiScanBatch take_scan_result() = 0;
  virtual void cancel_scan() = 0;
  [[nodiscard]] virtual unsigned scan_attempt() const { return 1U; }
  [[nodiscard]] virtual unsigned scan_limit() const { return 2U; }
};

enum class WifiScanState : std::uint8_t {
  idle,
  queued,
  scanning,
  ready,
  failed,
  cancelled,
  timed_out,
};

struct WifiScanSnapshot {
  std::uint32_t generation = 0U;
  WifiScanState state = WifiScanState::idle;
  std::uint8_t count = 0U;
  bool truncated = false;
  bool draining = false;
  std::uint8_t attempt = 0U;
  std::uint8_t maximum = 2U;
};

/**
 * loop thread가 소유하는 AP 검색 상태다.
 *
 * 목록은 다음 request/cancel까지만 유효하다. UI는 generation과 index로 조회하고 목록
 * 전체를 snapshot으로 복사하지 않는다.
 */
class WifiScanService {
 public:
  explicit WifiScanService(WifiScanAdapter& adapter) noexcept
      : adapter_(adapter) {}

  bool request() noexcept;
  void cancel() noexcept;
  void tick(std::uint32_t now_ms, bool allowed) noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] WifiScanSnapshot snapshot() const noexcept { return snapshot_; }
  [[nodiscard]] const WifiScanEntry* entry(std::uint32_t generation,
                                           std::size_t index) const noexcept;

 private:
  WifiScanAdapter& adapter_;
  std::unique_ptr<WifiScanEntry[]> entries_{};
  WifiScanSnapshot snapshot_{};
  std::uint32_t started_ms_ = 0U;
  bool active_ = false;
};

static_assert(sizeof(WifiScanEntry) <= 48U,
              "Scan entry must not own dynamic strings");
static_assert(sizeof(WifiScanSnapshot) <= 16U,
              "UI must not copy the scan list");
static_assert(sizeof(WifiScanService) <= 56U,
              "Scan list belongs on checked heap");

}  // namespace hmi::connectivity
