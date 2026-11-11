#include <cassert>
#include <cstdint>

#include "boards/jc1060p470c/internal/camera_frame_lease_tracker.hpp"

using hmi::boards::jc1060p470c::internal::CameraFrameLeaseTracker;

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

  assert(tracker.start());
  const auto restarted = tracker.acquire(300U, 100U);
  assert(restarted.valid);
  assert(tracker.release(restarted));
  assert(tracker.stop() == 0U);
  return 0;
}
