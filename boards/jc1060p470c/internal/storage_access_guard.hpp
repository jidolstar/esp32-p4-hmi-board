#pragma once

#include <atomic>
#include <cstdint>

namespace hmi::boards::jc1060p470c::internal {

/**
 * HMI 파일 접근 lease와 USB raw 접근을 하나의 atomic word로 중재한다.
 *
 * low bit 영역은 진행 중인 HMI lease 수, high bit는 USB raw owner다. 따라서 raw 전환과
 * lease 취득이 동시에 성공할 수 없고 callback/task 사이에서도 별도 mutex가 필요 없다.
 */
class StorageAccessGuard {
 public:
  static constexpr std::uint16_t kRawOwner = 0x8000U;
  static constexpr std::uint16_t kLeaseMask = 0x7FFFU;

  [[nodiscard]] bool try_acquire_file_lease() noexcept {
    std::uint16_t current = state_.load(std::memory_order_acquire);
    for (;;) {
      if ((current & kRawOwner) != 0U || (current & kLeaseMask) == kLeaseMask) {
        return false;
      }
      if (state_.compare_exchange_weak(current,
                                       static_cast<std::uint16_t>(current + 1U),
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
        return true;
      }
    }
  }

  void release_file_lease() noexcept {
    std::uint16_t current = state_.load(std::memory_order_acquire);
    for (;;) {
      if ((current & kRawOwner) != 0U || (current & kLeaseMask) == 0U) {
        return;
      }
      if (state_.compare_exchange_weak(current,
                                       static_cast<std::uint16_t>(current - 1U),
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
        return;
      }
    }
  }

  /** 진행 중인 HMI lease가 전혀 없을 때만 USB raw owner를 원자적으로 예약한다. */
  [[nodiscard]] bool try_begin_raw_access() noexcept {
    std::uint16_t expected = 0U;
    return state_.compare_exchange_strong(expected, kRawOwner,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
  }

  void end_raw_access() noexcept {
    std::uint16_t expected = kRawOwner;
    state_.compare_exchange_strong(expected, 0U, std::memory_order_acq_rel,
                                   std::memory_order_acquire);
  }

  [[nodiscard]] bool raw_access_active() const noexcept {
    return (state_.load(std::memory_order_acquire) & kRawOwner) != 0U;
  }

  [[nodiscard]] std::uint16_t file_lease_count() const noexcept {
    return static_cast<std::uint16_t>(
        state_.load(std::memory_order_acquire) & kLeaseMask);
  }

 private:
  std::atomic<std::uint16_t> state_{0U};
};

static_assert(sizeof(StorageAccessGuard) <= sizeof(std::uint32_t),
              "storage ownership guard must remain bounded");

}  // namespace hmi::boards::jc1060p470c::internal
