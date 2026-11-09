#include <cassert>
#include <cstdint>

#include "boards/jc1060p470c/internal/storage_access_guard.hpp"

using hmi::boards::jc1060p470c::internal::StorageAccessGuard;

int main() {
  StorageAccessGuard guard;
  assert(guard.file_lease_count() == 0U);
  assert(!guard.raw_access_active());

  // 여러 HMI 작업은 lease를 공유하지만 USB raw owner는 그동안 진입할 수 없다.
  assert(guard.try_acquire_file_lease());
  assert(guard.try_acquire_file_lease());
  assert(guard.file_lease_count() == 2U);
  assert(!guard.try_begin_raw_access());

  guard.release_file_lease();
  assert(!guard.try_begin_raw_access());
  guard.release_file_lease();
  assert(guard.file_lease_count() == 0U);

  // USB raw owner가 활성화되면 모든 새 HMI file lease를 안정적으로 거부한다.
  assert(guard.try_begin_raw_access());
  assert(guard.raw_access_active());
  assert(!guard.try_begin_raw_access());
  assert(!guard.try_acquire_file_lease());
  assert(guard.file_lease_count() == 0U);

  guard.end_raw_access();
  assert(!guard.raw_access_active());
  assert(guard.try_acquire_file_lease());
  guard.release_file_lease();

  // 잘못된 중복 release/end가 counter underflow나 owner 변경을 만들지 않는다.
  guard.release_file_lease();
  guard.end_raw_access();
  assert(guard.file_lease_count() == 0U);
  assert(!guard.raw_access_active());
  return 0;
}
