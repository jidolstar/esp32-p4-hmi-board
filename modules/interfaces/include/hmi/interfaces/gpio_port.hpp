#pragma once

#include <cstdint>

#include "hmi/interfaces/result.hpp"

namespace hmi::interfaces {

using GpioLine = std::uint16_t;

enum class GpioMode : std::uint8_t { input, output };
enum class GpioPull : std::uint8_t { none, up, down };

struct GpioConfig {
  GpioMode mode = GpioMode::input;
  GpioPull pull = GpioPull::none;
  bool initial_high = false;
};

/** 제품 pin map을 노출하지 않고 adapter의 논리 line을 제어하는 비소유 계약이다. */
struct GpioPort {
  void* context = nullptr;
  Result (*configure)(void*, GpioLine, const GpioConfig&) = nullptr;
  Result (*read)(void*, GpioLine, bool&) = nullptr;
  Result (*write)(void*, GpioLine, bool) = nullptr;
};

[[nodiscard]] GpioPort unsupported_gpio_port() noexcept;

static_assert(sizeof(GpioPort) <= 32U,
              "GPIO port must remain a small non-owning view");

}  // namespace hmi::interfaces
