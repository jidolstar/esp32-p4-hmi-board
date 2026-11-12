#include <cassert>
#include <cstdint>

#include "boards/jc1060p470c/internal/camera_frame_lease_tracker.hpp"
#include "boards/jc1060p470c/internal/csi_frame_slots.hpp"

using hmi::boards::jc1060p470c::internal::CameraFrameLeaseTracker;
using hmi::boards::jc1060p470c::internal::CsiFrameSlots;

int main() {
  CameraFrameLeaseTracker<2U> tracker;
  assert(!tracker.running());
  assert(!tracker.acquire(0U, 100U).valid);
  assert(tracker.start());
  assert(!tracker.start());

  const auto first = tracker.acquire(10U, 100U);
  const auto second = tracker.acquire(20U, 100U);
  assert(first.valid && second.valid);
  assert(first.slot != second.slot);
  assert(tracker.outstanding() == 2U);
  assert(!tracker.acquire(30U, 100U).valid);

  // 같은 token은 정확히 한 번만 driver buffer를 반환할 수 있다.
  assert(tracker.release(first));
  assert(!tracker.release(first));
  assert(tracker.outstanding() == 1U);

  // timeout 회수 뒤 도착한 callback은 stale generation이라 거부한다.
  assert(tracker.reclaim_expired(119U) == 0U);
  assert(tracker.reclaim_expired(120U) == 1U);
  assert(!tracker.release(second));
  assert(tracker.outstanding() == 0U);

  // uint32 millisecond wrap을 지나도 deadline 비교가 유지된다.
  const auto wrapped = tracker.acquire(UINT32_MAX - 4U, 10U);
  assert(wrapped.valid);
  assert(tracker.reclaim_expired(4U) == 0U);
  assert(tracker.reclaim_expired(5U) == 1U);

  const auto outstanding = tracker.acquire(200U, 100U);
  assert(outstanding.valid);
  assert(!tracker.start());
  assert(tracker.stop() == 1U);
  assert(!tracker.running());
  assert(tracker.outstanding() == 0U);
  assert(!tracker.release(outstanding));
  assert(tracker.stop() == 0U);

  CsiFrameSlots slots;
  assert(slots.borrow().id == 0U);
  const auto first_dma = slots.next_dma();
  const auto second_dma = slots.next_dma();
  assert(first_dma != second_dma);
  slots.complete(first_dma, true);
  const auto loan = slots.borrow();
  assert(loan.index == first_dma && loan.id != 0U);

  // CSI가 다음 DMA를 먼저 요청하는 동안 대여 중인 슬롯은 덮어쓰지 않는다.
  assert(slots.next_dma() == second_dma);
  slots.complete(second_dma, true);
  assert(slots.borrow().id == 0U && slots.borrowed());
  slots.release(loan.id);
  assert(slots.next_dma() == first_dma);
  slots.complete(second_dma, true);
  const auto next_loan = slots.borrow();
  assert(next_loan.index == second_dma && next_loan.id != loan.id);
  slots.release(loan.id);
  assert(slots.borrowed());
  slots.release(next_loan.id);
  assert(!slots.borrowed());

  slots.clear();
  const auto restarted_dma = slots.next_dma();
  const auto following_dma = slots.next_dma();
  slots.complete(restarted_dma, false);
  assert(slots.borrow().id == 0U);
  assert(slots.next_dma() == restarted_dma);
  slots.complete(following_dma, true);
  const auto restarted_loan = slots.borrow();
  assert(restarted_loan.id != 0U && restarted_loan.id != next_loan.id);
  slots.release(restarted_loan.id);

  assert(tracker.start());
  const auto restarted = tracker.acquire(300U, 100U);
  assert(restarted.valid);
  assert(tracker.release(restarted));
  assert(tracker.stop() == 0U);
  return 0;
}
