#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/interfaces/result.hpp"

namespace hmi::interfaces {

inline constexpr std::size_t kMaximumUsbCommandBytes = 512U;

/** USB reader가 command dispatcher와 공유하는 비소유 frame view다. */
struct UsbCommandPort {
  void* context = nullptr;
  Result (*receive)(void*, std::uint8_t*, std::size_t, std::size_t&) = nullptr;
  Result (*send)(void*, const std::uint8_t*, std::size_t,
                 std::size_t&) = nullptr;
  Result (*cancel)(void*) = nullptr;
};

[[nodiscard]] UsbCommandPort unsupported_usb_command_port() noexcept;

static_assert(sizeof(UsbCommandPort) <= 32U,
              "USB command port must remain a small non-owning view");

}  // namespace hmi::interfaces
