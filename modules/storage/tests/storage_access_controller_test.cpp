#include <cassert>
#include <cstdint>

#include "hmi/storage/storage_access_controller.hpp"
#include "storage/virtual_storage_access.hpp"

namespace {

using hmi::platform::windows::storage::VirtualStorageAccess;
using namespace hmi::storage;

void advance_to_waiting(VirtualStorageAccess& port,
                        StorageAccessController& controller,
                        std::uint32_t now_ms = 0U) {
  controller.tick(now_ms);
  assert(controller.snapshot().state == StorageAccessState::hmi_ready);
  assert(controller.request_pc_access(now_ms));
  assert(controller.snapshot().state == StorageAccessState::quiescing);
  assert(controller.snapshot().lock_required);
  assert(!controller.request_pc_access(now_ms));
  controller.tick(now_ms + 1U);
  assert(controller.snapshot().state == StorageAccessState::exposing);
  assert(port.complete_pending());
  controller.tick(now_ms + 2U);
  assert(controller.snapshot().state == StorageAccessState::waiting_for_host);
  assert(controller.snapshot().owner == StorageOwner::usb_msc);
}

void complete_reclaim(VirtualStorageAccess& port,
                      StorageAccessController& controller,
                      std::uint32_t now_ms) {
  controller.tick(now_ms);  // reclaim request dispatch
  assert(port.complete_pending());
  controller.tick(now_ms + 1U);
}

}  // namespace

int main() {
  {
    VirtualStorageAccess port;
    StorageAccessController controller(port);
    advance_to_waiting(port, controller);
    port.host_read();
    controller.tick(3U);
    assert(controller.snapshot().state == StorageAccessState::pc_active);
    assert(controller.snapshot().host_seen);

    port.usb_suspend();
    controller.tick(4U);
    assert(controller.snapshot().state == StorageAccessState::pc_active);
    assert(controller.snapshot().suspended);
    port.usb_resume();

    port.host_eject();
    controller.tick(5U);
    assert(controller.snapshot().state == StorageAccessState::reclaiming);
    assert(controller.snapshot().reclaim_reason ==
           StorageReclaimReason::host_eject);
    complete_reclaim(port, controller, 6U);
    const auto restored = controller.snapshot();
    assert(restored.state == StorageAccessState::hmi_ready);
    assert(!restored.lock_required);
    assert(restored.rescan_revision == 1U);
    assert(restored.generation == 2U);
  }

  {
    VirtualStorageAccess port;
    StorageAccessController controller(port);
    advance_to_waiting(port, controller, 100U);
    port.host_read();
    controller.tick(103U);
    port.usb_unplug();
    controller.tick(104U);
    assert(controller.snapshot().state == StorageAccessState::pc_active);
    controller.tick(303U);
    assert(controller.snapshot().state == StorageAccessState::pc_active);
    controller.tick(304U);
    assert(controller.snapshot().state == StorageAccessState::reclaiming);
    assert(controller.snapshot().reclaim_reason ==
           StorageReclaimReason::usb_unplug);
    complete_reclaim(port, controller, 305U);
    assert(controller.snapshot().state == StorageAccessState::hmi_ready);
  }

  {
    VirtualStorageAccess port;
    StorageAccessController controller(port);
    advance_to_waiting(port, controller);
    assert(controller.request_hmi_access(
        3U, StorageReclaimReason::explicit_request));
    assert(!controller.request_hmi_access(
        3U, StorageReclaimReason::explicit_request));
    complete_reclaim(port, controller, 4U);
    assert(controller.snapshot().state == StorageAccessState::hmi_ready);
  }

  {
    VirtualStorageAccess port;
    StorageAccessController controller(port);
    controller.tick(0U);
    assert(controller.request_pc_access(0U));
    controller.tick(1U);
    port.set_next_result(StoragePortResult::busy);
    assert(port.complete_pending());
    controller.tick(2U);
    assert(controller.snapshot().state == StorageAccessState::hmi_ready);
    assert(controller.snapshot().error == StorageAccessError::busy);
    assert(!controller.snapshot().lock_required);
  }

  {
    VirtualStorageAccess port;
    StorageAccessController controller(port);
    advance_to_waiting(port, controller);
    assert(controller.request_hmi_access(
        3U, StorageReclaimReason::explicit_request));
    controller.tick(4U);
    port.set_next_result(StoragePortResult::remount_failed);
    assert(port.complete_pending());
    controller.tick(5U);
    assert(controller.snapshot().state == StorageAccessState::remount_failed);
    assert(controller.snapshot().lock_required);
    assert(controller.snapshot().rescan_revision == 0U);

    assert(controller.retry_remount(6U));
    assert(!controller.retry_remount(6U));
    controller.tick(7U);
    assert(port.complete_pending());
    controller.tick(8U);
    assert(controller.snapshot().state == StorageAccessState::hmi_ready);
    assert(controller.snapshot().rescan_revision == 1U);
  }

  {
    VirtualStorageAccess port;
    StorageAccessController controller(port);
    advance_to_waiting(port, controller);
    assert(controller.request_hmi_access(
        3U, StorageReclaimReason::explicit_request));
    controller.tick(4U);
    port.set_next_result(StoragePortResult::callback_drain_timeout);
    assert(port.complete_pending());
    controller.tick(5U);
    assert(controller.snapshot().state == StorageAccessState::remount_failed);
    assert(controller.snapshot().error ==
           StorageAccessError::callback_drain_timeout);
    assert(controller.snapshot().owner == StorageOwner::usb_msc);
  }

  {
    VirtualStorageAccess port;
    StorageAccessController controller(port);
    controller.tick(0U);
    assert(controller.request_pc_access(0U));
    controller.tick(1U);
    const std::uint32_t token = port.snapshot().operation_token;
    port.inject_completion(StoragePortOperation::reclaim, token,
                           StoragePortResult::success);
    controller.tick(2U);
    assert(controller.snapshot().state == StorageAccessState::exposing);
    port.inject_completion(StoragePortOperation::expose, token + 1U,
                           StoragePortResult::success);
    controller.tick(3U);
    assert(controller.snapshot().state == StorageAccessState::exposing);
    assert(port.complete_pending());
    controller.tick(4U);
    assert(controller.snapshot().state == StorageAccessState::waiting_for_host);
  }

  {
    VirtualStorageAccess port;
    port.set_media_present(false);
    StorageAccessController controller(port);
    controller.tick(0U);
    assert(controller.snapshot().state == StorageAccessState::unavailable);
    assert(controller.snapshot().error == StorageAccessError::no_media);
    assert(!controller.request_pc_access(0U));
    assert(controller.retry_media(1U));
    assert(!controller.snapshot().lock_required);
    controller.tick(2U);
    port.set_next_result(StoragePortResult::unavailable);
    assert(port.complete_pending());
    controller.tick(3U);
    assert(controller.snapshot().state == StorageAccessState::unavailable);
    assert(controller.snapshot().error == StorageAccessError::no_media);
    assert(!controller.snapshot().lock_required);

    assert(controller.retry_media(4U));
    controller.tick(5U);
    assert(port.complete_pending());
    controller.tick(6U);
    assert(controller.snapshot().state == StorageAccessState::hmi_ready);
    assert(controller.snapshot().rescan_revision == 1U);
    assert(!controller.snapshot().lock_required);
  }
  return 0;
}
