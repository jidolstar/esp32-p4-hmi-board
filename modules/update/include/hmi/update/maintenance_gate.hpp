#pragma once

#include <atomic>
#include <cstdint>

namespace hmi::update {

/** update 작업의 단일 owner를 보장하는 lock-free 상태 gate다. */
class MaintenanceGate {
 public:
  [[nodiscard]] bool try_enter() noexcept {
    std::uint8_t expected = 0U;
    return state_.compare_exchange_strong(expected, 1U,
                                          std::memory_order_acq_rel);
  }

  void leave() noexcept { state_.store(0U, std::memory_order_release); }

  [[nodiscard]] bool active() const noexcept {
    return state_.load(std::memory_order_acquire) != 0U;
  }

 private:
  std::atomic<std::uint8_t> state_{0U};
};

static_assert(sizeof(MaintenanceGate) <= 4U,
              "maintenance gate must remain one atomic state");

}  // namespace hmi::update
