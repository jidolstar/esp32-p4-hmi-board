#pragma once

#include <cstdint>

#include "hmi/interfaces/result.hpp"

namespace hmi::interfaces {

using PwmChannel = std::uint8_t;

struct PwmConfig {
  std::uint32_t frequency_hz = 0U;
  std::uint8_t resolution_bits = 0U;
};

/** 실제 timer/channel handle을 숨기고 bounded channel만 다루는 비소유 PWM 계약이다. */
struct PwmPort {
  void* context = nullptr;
  Result (*attach)(void*, PwmChannel, const PwmConfig&) = nullptr;
  Result (*set_duty)(void*, PwmChannel, std::uint32_t) = nullptr;
  Result (*stop)(void*, PwmChannel) = nullptr;
  Result (*detach)(void*, PwmChannel) = nullptr;
};

[[nodiscard]] PwmPort unsupported_pwm_port() noexcept;

static_assert(sizeof(PwmPort) <= 40U,
              "PWM port must remain a small non-owning view");

}  // namespace hmi::interfaces
