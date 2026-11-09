#pragma once

#include <cstdint>

namespace hmi::boards::jc1060p470c {

/** 보드 touch driver와 상위 입력 port 사이에서 사용하는 작은 좌표 snapshot이다. */
struct TouchPoint {
  bool pressed = false;
  std::int16_t x = 0;
  std::int16_t y = 0;
};

}  // namespace hmi::boards::jc1060p470c
