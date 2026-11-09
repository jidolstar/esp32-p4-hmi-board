#include "internal/touch_controller.hpp"

#include <Arduino.h>
#include <Wire.h>

#include "config.hpp"
#include "internal/i2c_transaction.hpp"
#include "log.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

constexpr std::uint8_t kPrimaryAddress = 0x5D;
constexpr std::uint8_t kBackupAddress = 0x14;
constexpr std::uint16_t kProductIdRegister = 0x8140;
constexpr std::uint16_t kStatusRegister = 0x814E;
constexpr std::uint8_t kMaximumPointCount = 5;

}  // namespace

TouchController::TouchController(TwoWire &wire) : wire_(wire) {}

bool TouchController::begin() {
  reset_for_primary_address();
  if (probe(kPrimaryAddress) || probe(kBackupAddress)) {
    HMI_LOG_INFO("[touch] GT911 ready address=0x%02x\r\n", address_);
    return true;
  }
  address_ = 0;
  HMI_LOG_ERROR("[touch] GT911 probe failed\r\n");
  return false;
}

TouchPoint TouchController::read() {
  TouchPoint point = last_touch_;
  if (read_point(&point)) {
    last_touch_ = point;
  }
  return last_touch_;
}

TouchPoint TouchController::last_touch() const { return last_touch_; }

bool TouchController::write(std::uint16_t reg, std::uint8_t value) {
  I2cTransaction transaction(20);
  if (!transaction.acquired()) {
    return false;
  }
  wire_.beginTransmission(address_);
  wire_.write(static_cast<std::uint8_t>(reg >> 8));
  wire_.write(static_cast<std::uint8_t>(reg));
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

bool TouchController::read_reg(std::uint16_t reg, std::uint8_t *data,
                               std::size_t length) {
  I2cTransaction transaction(20);
  if (!transaction.acquired()) {
    return false;
  }
  wire_.beginTransmission(address_);
  wire_.write(static_cast<std::uint8_t>(reg >> 8));
  wire_.write(static_cast<std::uint8_t>(reg));
  if (wire_.endTransmission(false) != 0) {
    return false;
  }
  if (wire_.requestFrom(address_, length) != length) {
    while (wire_.available()) {
      wire_.read();
    }
    return false;
  }
  for (std::size_t index = 0; index < length; ++index) {
    data[index] = wire_.read();
  }
  return true;
}

bool TouchController::probe(std::uint8_t address) {
  address_ = address;
  std::uint8_t product_id[4] = {};
  if (!read_reg(kProductIdRegister, product_id, sizeof(product_id))) {
    return false;
  }
  return product_id[0] != 0 && product_id[0] != 0xFF;
}

void TouchController::reset_for_primary_address() {
  pinMode(config::kTouchReset, OUTPUT);
  pinMode(config::kTouchIrq, OUTPUT);
  digitalWrite(config::kTouchReset, LOW);
  digitalWrite(config::kTouchIrq, LOW);
  delay(10);
  digitalWrite(config::kTouchReset, HIGH);
  delay(60);
  pinMode(config::kTouchIrq, INPUT_PULLUP);
  delay(50);
}

bool TouchController::read_point(TouchPoint *point) {
  if (point == nullptr || address_ == 0) {
    return false;
  }

  std::uint8_t status = 0;
  if (!read_reg(kStatusRegister, &status, 1)) {
    return false;
  }
  if ((status & 0x80U) == 0) {
    return true;
  }

  const std::uint8_t count = status & 0x0FU;
  point->pressed = false;
  if (count > 0 && count <= kMaximumPointCount) {
    // GT911의 첫 pointer record만 읽으며 stack 사용량을 8바이트로 제한한다.
    std::uint8_t data[8] = {};
    if (!read_reg(kStatusRegister + 1, data, sizeof(data))) {
      return false;
    }
    const std::uint16_t x =
        data[1] | (static_cast<std::uint16_t>(data[2]) << 8);
    const std::uint16_t y =
        data[3] | (static_cast<std::uint16_t>(data[4]) << 8);
    point->pressed = true;
    point->x = x < config::kScreenWidth ? x : config::kScreenWidth - 1;
    point->y = y < config::kScreenHeight ? y : config::kScreenHeight - 1;
  }
  return write(kStatusRegister, 0);
}

}  // namespace hmi::boards::jc1060p470c::internal
