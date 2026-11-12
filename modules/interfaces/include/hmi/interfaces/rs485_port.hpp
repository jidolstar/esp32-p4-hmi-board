#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/interfaces/result.hpp"

namespace hmi::interfaces {

enum class SerialParity : std::uint8_t { none, even, odd };

struct Rs485Config {
  std::uint32_t baud_rate = 0U;
  SerialParity parity = SerialParity::none;
  std::uint8_t data_bits = 8U;
  std::uint8_t stop_bits = 1U;
};

/**
 * RS485 adapter가 제공하는 비소유 호출 table이다.
 *
 * context와 callback은 port를 사용하는 controller보다 오래 살아야 한다. frame buffer는 호출
 * 동안만 유효하며 구현이 보관해서는 안 된다. UART 번호와 direction pin은 adapter 내부에 둔다.
 */
struct Rs485Port {
  void* context = nullptr;
  Result (*configure)(void*, const Rs485Config&) = nullptr;
  Result (*write)(void*, const std::uint8_t*, std::size_t,
                  std::size_t&) = nullptr;
  Result (*read)(void*, std::uint8_t*, std::size_t, std::size_t&) = nullptr;
  Result (*cancel)(void*) = nullptr;
};

[[nodiscard]] Rs485Port unsupported_rs485_port() noexcept;

static_assert(sizeof(Rs485Port) <= 40U,
              "RS485 port must remain a small non-owning view");

}  // namespace hmi::interfaces
