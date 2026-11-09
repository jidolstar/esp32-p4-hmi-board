#include <cassert>

#include "boards/jc1060p470c/internal/usb_msc_event_state.hpp"

using hmi::boards::jc1060p470c::internal::UsbBusSignal;
using hmi::boards::jc1060p470c::internal::UsbMscEventState;

int main() {
  UsbMscEventState state;
  state.reset();
  state.set_active(true);
  state.set_media_present(true);

  // USB bus STARTED만으로 PC가 MSC media를 인식했다고 판단하지 않는다.
  state.on_bus_signal(UsbBusSignal::started);
  auto snapshot = state.snapshot();
  assert(snapshot.usb_started);
  assert(!snapshot.host_seen);

  // suspend는 cable unplug가 아니며 stopped/eject를 만들지 않는다.
  state.on_bus_signal(UsbBusSignal::suspended);
  snapshot = state.snapshot();
  assert(snapshot.suspended);
  assert(!snapshot.usb_stopped);
  assert(!snapshot.host_ejected);
  state.on_bus_signal(UsbBusSignal::resumed);
  assert(!state.snapshot().suspended);

  // 첫 실제 I/O가 host_seen을 고정하고 in-flight drain 근거를 제공한다.
  assert(state.try_begin_io());
  snapshot = state.snapshot();
  assert(snapshot.host_seen);
  assert(snapshot.host_active);
  assert(snapshot.in_flight == 1U);
  state.set_media_present(false);
  assert(!state.try_begin_io());
  assert(state.snapshot().in_flight == 1U);
  state.end_io();
  assert(state.snapshot().in_flight == 0U);

  state.set_media_present(true);
  state.on_start_stop(false, true);
  snapshot = state.snapshot();
  assert(snapshot.host_ejected);
  assert(!snapshot.host_active);

  state.on_bus_signal(UsbBusSignal::stopped);
  snapshot = state.snapshot();
  assert(snapshot.usb_stopped);
  assert(snapshot.host_seen);
  return 0;
}
