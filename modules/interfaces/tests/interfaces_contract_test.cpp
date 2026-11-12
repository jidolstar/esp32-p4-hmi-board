#include <cassert>
#include <cstddef>
#include <cstdint>

#include "hmi/interfaces/capability.hpp"
#include "hmi/interfaces/gpio_port.hpp"
#include "hmi/interfaces/pwm_port.hpp"
#include "hmi/interfaces/rs485_port.hpp"
#include "hmi/interfaces/usb_command_port.hpp"

int main() {
  using namespace hmi::interfaces;

  assert(succeeded(Result::ok));
  assert(!succeeded(Result::busy));
  assert(valid({CapabilityId::gpio, SupportLevel::contract_only, 4U, 0U}));
  assert(valid({CapabilityId::rs485, SupportLevel::available, 1U, 256U}));
  assert(!valid({CapabilityId::pwm, SupportLevel::available, 0U, 0U}));

  std::size_t transferred = 99U;
  std::uint8_t bytes[8]{};
  const auto rs485 = unsupported_rs485_port();
  assert(rs485.configure(rs485.context, {}) == Result::not_implemented);
  assert(rs485.write(rs485.context, bytes, sizeof(bytes), transferred) ==
         Result::not_implemented);
  assert(transferred == 0U);
  assert(rs485.read(rs485.context, bytes, sizeof(bytes), transferred) ==
         Result::not_implemented);
  assert(rs485.cancel(rs485.context) == Result::not_implemented);

  bool high = true;
  const auto gpio = unsupported_gpio_port();
  assert(gpio.configure(gpio.context, 2U, {}) == Result::not_implemented);
  assert(gpio.read(gpio.context, 2U, high) == Result::not_implemented);
  assert(!high);
  assert(gpio.write(gpio.context, 2U, true) == Result::not_implemented);

  const auto pwm = unsupported_pwm_port();
  assert(pwm.attach(pwm.context, 1U, {}) == Result::not_implemented);
  assert(pwm.set_duty(pwm.context, 1U, 100U) == Result::not_implemented);
  assert(pwm.stop(pwm.context, 1U) == Result::not_implemented);
  assert(pwm.detach(pwm.context, 1U) == Result::not_implemented);

  transferred = 99U;
  const auto usb = unsupported_usb_command_port();
  assert(usb.receive(usb.context, bytes, sizeof(bytes), transferred) ==
         Result::not_implemented);
  assert(transferred == 0U);
  assert(usb.send(usb.context, bytes, sizeof(bytes), transferred) ==
         Result::not_implemented);
  assert(usb.cancel(usb.context) == Result::not_implemented);

  return 0;
}
