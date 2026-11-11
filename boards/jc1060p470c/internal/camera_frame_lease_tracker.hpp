#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace hmi::boards::jc1060p470c::internal {

/**
 * Camera driver buffer의 대여 상태만 추적하는 고정 크기 helper다.
 *
 * 실제 frame pointer와 driver handle은 보관하지 않는다. 세대 번호를 token에 넣어 timeout,
 * stop 또는 이미 반환된 frame의 늦은 반환이 새 lease를 해제하지 못하게 한다.
 */
template <std::size_t SlotCount>
class CameraFrameLeaseTracker {
  static_assert(SlotCount > 0U, "camera frame pool needs at least one slot");
  static_assert(SlotCount <= std::numeric_limits<std::uint8_t>::max(),
                "camera frame token slot is uint8_t");

 public:
  struct Token {
    std::uint32_t generation = 0U;
    std::uint8_t slot = 0U;
    bool valid = false;
  };

  /** 새 stream을 시작한다. outstanding lease가 있으면 시작하지 않는다. */
  bool start() noexcept {
    if (running_ || outstanding_ != 0U) {
      return false;
    }
    running_ = true;
    return true;
  }

  /** stream을 중지하고 반환되지 않은 token을 모두 무효화한다. */
  std::size_t stop() noexcept {
    running_ = false;
    const auto released = outstanding_;
    for (auto& slot : slots_) {
      if (slot.leased) {
        slot.leased = false;
        ++slot.generation;
      }
    }
    outstanding_ = 0U;
    return released;
  }

  /** timeout을 포함한 lease를 만든다. 빈 slot이나 실행 중 stream이 없으면 invalid다. */
  [[nodiscard]] Token acquire(std::uint32_t now_ms,
                              std::uint32_t timeout_ms) noexcept {
    if (!running_ || timeout_ms == 0U) {
      return {};
    }
    for (std::size_t index = 0U; index < slots_.size(); ++index) {
      auto& slot = slots_[index];
      if (!slot.leased) {
        slot.leased = true;
        slot.deadline_ms = now_ms + timeout_ms;
        ++outstanding_;
        return {slot.generation, static_cast<std::uint8_t>(index), true};
      }
    }
    return {};
  }

  /** 정확히 한 번만 lease를 반환한다. stale/duplicate token은 거부한다. */
  bool release(Token token) noexcept {
    if (!matches(token)) {
      return false;
    }
    auto& slot = slots_[token.slot];
    slot.leased = false;
    ++slot.generation;
    --outstanding_;
    return true;
  }

  /** deadline이 지난 lease를 무효화하고 driver가 회수해야 할 개수를 반환한다. */
  std::size_t reclaim_expired(std::uint32_t now_ms) noexcept {
    std::size_t reclaimed = 0U;
    for (auto& slot : slots_) {
      if (slot.leased && deadline_reached(now_ms, slot.deadline_ms)) {
        slot.leased = false;
        ++slot.generation;
        --outstanding_;
        ++reclaimed;
      }
    }
    return reclaimed;
  }

  [[nodiscard]] bool running() const noexcept { return running_; }
  [[nodiscard]] std::size_t outstanding() const noexcept {
    return outstanding_;
  }

 private:
  struct Slot {
    std::uint32_t generation = 1U;
    std::uint32_t deadline_ms = 0U;
    bool leased = false;
  };

  [[nodiscard]] bool matches(Token token) const noexcept {
    if (!token.valid || token.slot >= slots_.size()) {
      return false;
    }
    const auto& slot = slots_[token.slot];
    return slot.leased && slot.generation == token.generation;
  }

  static bool deadline_reached(std::uint32_t now_ms,
                               std::uint32_t deadline_ms) noexcept {
    return static_cast<std::int32_t>(now_ms - deadline_ms) >= 0;
  }

  std::array<Slot, SlotCount> slots_{};
  std::size_t outstanding_ = 0U;
  bool running_ = false;
};

}  // namespace hmi::boards::jc1060p470c::internal
