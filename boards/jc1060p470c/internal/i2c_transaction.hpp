#pragma once

#include <cstdint>

namespace hmi::boards::jc1060p470c::internal {

/** 공유 I2C의 register 선택부터 read/write 완료까지 하나의 transaction으로 보호한다. */
class I2cTransaction {
 public:
  explicit I2cTransaction(std::uint32_t timeout_ms) noexcept;
  ~I2cTransaction();
  I2cTransaction(const I2cTransaction &) = delete;
  I2cTransaction &operator=(const I2cTransaction &) = delete;

  bool acquired() const noexcept { return acquired_; }

 private:
  bool acquired_ = false;
};

}  // namespace hmi::boards::jc1060p470c::internal
