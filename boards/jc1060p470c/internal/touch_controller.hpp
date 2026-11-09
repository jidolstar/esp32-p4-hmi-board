#pragma once

#include <cstddef>
#include <cstdint>

#include "touch_point.hpp"

class TwoWire;

namespace hmi::boards::jc1060p470c::internal {

/**
 * GT911 초기화와 단일 pointer 좌표 읽기를 소유한다.
 *
 * 공유 I2C의 lifetime은 Board가 소유한다. 이 객체는 Wire를 빌려 쓰며 LVGL이나
 * 화면 객체를 알지 않는다.
 */
class TouchController {
 public:
  explicit TouchController(TwoWire &wire);

  bool begin();
  TouchPoint read();
  TouchPoint last_touch() const;

 private:
  bool write(std::uint16_t reg, std::uint8_t value);
  bool read_reg(std::uint16_t reg, std::uint8_t *data, std::size_t length);
  bool probe(std::uint8_t address);
  void reset_for_primary_address();
  bool read_point(TouchPoint *point);

  TwoWire &wire_;
  TouchPoint last_touch_;
  std::uint8_t address_ = 0;
};

}  // namespace hmi::boards::jc1060p470c::internal
