#include "internal/i2c_transaction.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace hmi::boards::jc1060p470c::internal {
namespace {

// 정적 buffer를 사용해 startup 중 mutex heap allocation을 만들지 않는다.
StaticSemaphore_t mutex_buffer;

SemaphoreHandle_t mutex_handle() noexcept {
  static SemaphoreHandle_t handle = xSemaphoreCreateMutexStatic(&mutex_buffer);
  return handle;
}

}  // namespace

I2cTransaction::I2cTransaction(std::uint32_t timeout_ms) noexcept {
  const SemaphoreHandle_t handle = mutex_handle();
  acquired_ = handle != nullptr &&
              xSemaphoreTake(handle, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

I2cTransaction::~I2cTransaction() {
  if (acquired_) {
    xSemaphoreGive(mutex_handle());
  }
}

}  // namespace hmi::boards::jc1060p470c::internal
