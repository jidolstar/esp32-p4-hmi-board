#include "hmi/interfaces/capability.hpp"

#include "hmi/interfaces/gpio_port.hpp"
#include "hmi/interfaces/pwm_port.hpp"
#include "hmi/interfaces/rs485_port.hpp"
#include "hmi/interfaces/usb_command_port.hpp"

namespace hmi::interfaces {
namespace {

Result unsupported_simple(void*) noexcept { return Result::not_implemented; }

Result unsupported_rs485_configure(void*, const Rs485Config&) noexcept {
  return Result::not_implemented;
}

Result unsupported_rs485_write(void*, const std::uint8_t*, std::size_t,
                               std::size_t& transferred) noexcept {
  transferred = 0U;
  return Result::not_implemented;
}

Result unsupported_rs485_read(void*, std::uint8_t*, std::size_t,
                              std::size_t& transferred) noexcept {
  transferred = 0U;
  return Result::not_implemented;
}

Result unsupported_gpio_configure(void*, GpioLine,
                                  const GpioConfig&) noexcept {
  return Result::not_implemented;
}

Result unsupported_gpio_read(void*, GpioLine, bool& high) noexcept {
  high = false;
  return Result::not_implemented;
}

Result unsupported_gpio_write(void*, GpioLine, bool) noexcept {
  return Result::not_implemented;
}

Result unsupported_pwm_attach(void*, PwmChannel, const PwmConfig&) noexcept {
  return Result::not_implemented;
}

Result unsupported_pwm_channel(void*, PwmChannel) noexcept {
  return Result::not_implemented;
}

Result unsupported_pwm_duty(void*, PwmChannel, std::uint32_t) noexcept {
  return Result::not_implemented;
}

Result unsupported_usb_receive(void*, std::uint8_t*, std::size_t,
                               std::size_t& transferred) noexcept {
  transferred = 0U;
  return Result::not_implemented;
}

Result unsupported_usb_send(void*, const std::uint8_t*, std::size_t,
                            std::size_t& transferred) noexcept {
  transferred = 0U;
  return Result::not_implemented;
}

}  // namespace

bool valid(const CapabilitySnapshot& value) noexcept {
  if (value.support == SupportLevel::unavailable) {
    return value.channel_count == 0U && value.maximum_payload_bytes == 0U;
  }
  if (value.id == CapabilityId::rs485 ||
      value.id == CapabilityId::usb_command) {
    return value.channel_count > 0U && value.maximum_payload_bytes > 0U;
  }
  return value.channel_count > 0U && value.maximum_payload_bytes == 0U;
}

Rs485Port unsupported_rs485_port() noexcept {
  return {nullptr, &unsupported_rs485_configure, &unsupported_rs485_write,
          &unsupported_rs485_read, &unsupported_simple};
}

GpioPort unsupported_gpio_port() noexcept {
  return {nullptr, &unsupported_gpio_configure, &unsupported_gpio_read,
          &unsupported_gpio_write};
}

PwmPort unsupported_pwm_port() noexcept {
  return {nullptr, &unsupported_pwm_attach, &unsupported_pwm_duty,
          &unsupported_pwm_channel, &unsupported_pwm_channel};
}

UsbCommandPort unsupported_usb_command_port() noexcept {
  return {nullptr, &unsupported_usb_receive, &unsupported_usb_send,
          &unsupported_simple};
}

}  // namespace hmi::interfaces
