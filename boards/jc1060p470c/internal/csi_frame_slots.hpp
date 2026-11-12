#pragma once

#include <array>
#include <cstdint>

namespace hmi::boards::jc1060p470c::internal {

/** CSI는 이전 전송 완료를 알리기 전에 다음 DMA 슬롯을 요청한다. 호출자가 ISR과 task를 직렬화한다. */
class CsiFrameSlots {
 public:
  static constexpr std::uint8_t kCount = 2U;
  struct Loan { std::uint8_t index = 0U; std::uint32_t id = 0U; };

  [[nodiscard]] std::uint8_t next_dma() noexcept {
    for (std::uint8_t i = 0U; i < kCount; ++i) {
      if (slots_[i].state == State::free || slots_[i].state == State::ready) {
        writing_ = i;
        break;
      }
    }
    // 다른 슬롯이 대여 중이면 현재 슬롯을 다시 쓰고 그 프레임은 공개하지 않는다.
    slots_[writing_].state = State::writing;
    return writing_;
  }

  void complete(std::uint8_t index, bool valid) noexcept {
    if (index >= kCount || index == writing_ || slots_[index].state != State::writing) {
      return;
    }
    auto& slot = slots_[index];
    if (!valid) {
      slot.state = State::free;
      return;
    }
    if (++sequence_ == 0U) {
      ++sequence_;
    }
    slot.id = sequence_;
    slot.state = State::ready;
  }

  [[nodiscard]] Loan borrow() noexcept {
    for (std::uint8_t i = 0U; i < kCount; ++i) {
      if (slots_[i].state == State::ready) {
        slots_[i].state = State::borrowed;
        return {i, slots_[i].id};
      }
    }
    return {};
  }

  void release(std::uint32_t id) noexcept {
    for (auto& slot : slots_) {
      if (slot.state == State::borrowed && slot.id == id) {
        slot.state = State::free;
        return;
      }
    }
  }

  [[nodiscard]] bool borrowed() const noexcept {
    return slots_[0].state == State::borrowed || slots_[1].state == State::borrowed;
  }

  /** DMA 정지 후에만 호출한다. 다음 스트림에서 이전 대여 ID는 계속 무효다. */
  void clear() noexcept { slots_ = {}; writing_ = 0U; }

 private:
  enum class State : std::uint8_t { free, writing, ready, borrowed };
  struct Slot { std::uint32_t id = 0U; State state = State::free; };

  std::array<Slot, kCount> slots_{};
  std::uint32_t sequence_ = 0U;
  std::uint8_t writing_ = 0U;
};

static_assert(sizeof(CsiFrameSlots) <= 32U, "two CSI slots must remain bounded");

}  // namespace hmi::boards::jc1060p470c::internal
