#pragma once

#include <cstdint>

namespace hmi::interfaces {

enum class CapabilityId : std::uint8_t { rs485, gpio, pwm, usb_command };

enum class SupportLevel : std::uint8_t {
  unavailable,
  contract_only,
  available,
};

/** UI나 진단 코드가 내부 handle 없이 기능 제공 수준을 읽는 작은 snapshot이다. */
struct CapabilitySnapshot {
  CapabilityId id = CapabilityId::rs485;
  SupportLevel support = SupportLevel::unavailable;
  std::uint16_t channel_count = 0U;
  std::uint32_t maximum_payload_bytes = 0U;
};

[[nodiscard]] bool valid(const CapabilitySnapshot& value) noexcept;

static_assert(sizeof(CapabilitySnapshot) <= 12U,
              "interface capability snapshot must remain bounded");

}  // namespace hmi::interfaces
