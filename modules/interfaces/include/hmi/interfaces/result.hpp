#pragma once

#include <cstdint>

namespace hmi::interfaces {

/** 장치별 오류 형식을 공통 호출자가 처리할 수 있게 축약한 결과다. */
enum class Result : std::uint8_t {
  ok,
  not_implemented,
  busy,
  invalid_argument,
  io_error,
};

[[nodiscard]] constexpr bool succeeded(Result value) noexcept {
  return value == Result::ok;
}

}  // namespace hmi::interfaces
