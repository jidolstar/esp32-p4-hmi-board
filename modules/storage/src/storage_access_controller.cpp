#include "hmi/storage/storage_access_controller.hpp"

#include <cstdint>

namespace hmi::storage {
namespace {

constexpr std::uint32_t kUsbDisconnectGraceMs = 200U;

StorageAccessError map_error(StoragePortResult result) noexcept {
  switch (result) {
    case StoragePortResult::unavailable:
      return StorageAccessError::no_media;
    case StoragePortResult::busy:
      return StorageAccessError::busy;
    case StoragePortResult::start_failed:
      return StorageAccessError::msc_start_failed;
    case StoragePortResult::callback_drain_timeout:
      return StorageAccessError::callback_drain_timeout;
    case StoragePortResult::remount_failed:
      return StorageAccessError::remount_failed;
    case StoragePortResult::none:
    case StoragePortResult::in_progress:
    case StoragePortResult::success:
      return StorageAccessError::none;
  }
  return StorageAccessError::port_rejected;
}

}  // namespace

bool StorageAccessController::request_pc_access(std::uint32_t) noexcept {
  if (snapshot_.state != StorageAccessState::hmi_ready ||
      pending_operation_ != StoragePortOperation::none) {
    return false;
  }
  snapshot_.state = StorageAccessState::quiescing;
  snapshot_.lock_required = true;
  snapshot_.error = StorageAccessError::none;
  pending_operation_ = StoragePortOperation::expose;
  operation_dispatched_ = false;
  ++operation_token_;
  return true;
}

bool StorageAccessController::request_hmi_access(
    std::uint32_t, StorageReclaimReason reason) noexcept {
  if ((snapshot_.state != StorageAccessState::waiting_for_host &&
       snapshot_.state != StorageAccessState::pc_active) ||
      pending_operation_ != StoragePortOperation::none) {
    return false;
  }
  begin_reclaim(reason);
  return true;
}

bool StorageAccessController::retry_remount(std::uint32_t) noexcept {
  if (snapshot_.state != StorageAccessState::remount_failed ||
      pending_operation_ != StoragePortOperation::none) {
    return false;
  }
  snapshot_.state = StorageAccessState::reclaiming;
  snapshot_.lock_required = true;
  snapshot_.error = StorageAccessError::none;
  pending_operation_ = StoragePortOperation::remount;
  operation_dispatched_ = false;
  ++operation_token_;
  return true;
}

bool StorageAccessController::retry_media(std::uint32_t) noexcept {
  if (snapshot_.state != StorageAccessState::unavailable ||
      pending_operation_ != StoragePortOperation::none) {
    return false;
  }
  snapshot_.error = StorageAccessError::none;
  pending_operation_ = StoragePortOperation::remount;
  operation_dispatched_ = false;
  media_probe_ = true;
  ++operation_token_;
  return true;
}

void StorageAccessController::tick(std::uint32_t now_ms) noexcept {
  port_.tick(now_ms);
  const StoragePortSnapshot port_snapshot = port_.snapshot();
  refresh(port_snapshot);

  if (!initialized_) {
    initialized_ = true;
    observed_completion_revision_ = port_snapshot.completion_revision;
    snapshot_.state = port_snapshot.mounted &&
                              port_snapshot.owner == StorageOwner::hmi
                          ? StorageAccessState::hmi_ready
                          : StorageAccessState::unavailable;
    snapshot_.error = snapshot_.state == StorageAccessState::unavailable
                          ? StorageAccessError::no_media
                          : StorageAccessError::none;
  }

  if (port_snapshot.completion_revision != observed_completion_revision_) {
    observed_completion_revision_ = port_snapshot.completion_revision;
    handle_completion(port_snapshot);
  }

  if (pending_operation_ != StoragePortOperation::none &&
      !operation_dispatched_) {
    dispatch(pending_operation_);
  }

  if (snapshot_.state == StorageAccessState::waiting_for_host &&
      port_snapshot.host_seen) {
    snapshot_.state = StorageAccessState::pc_active;
  }

  if ((snapshot_.state == StorageAccessState::waiting_for_host ||
       snapshot_.state == StorageAccessState::pc_active) &&
      pending_operation_ == StoragePortOperation::none) {
    if (port_snapshot.host_ejected) {
      begin_reclaim(StorageReclaimReason::host_eject);
    } else if (port_snapshot.usb_stopped && port_snapshot.host_seen) {
      if (!disconnect_grace_active_) {
        disconnect_grace_active_ = true;
        disconnect_deadline_ms_ = now_ms + kUsbDisconnectGraceMs;
      } else if (time_reached(now_ms, disconnect_deadline_ms_)) {
        begin_reclaim(StorageReclaimReason::usb_unplug);
      }
    }
  }

  snapshot_.lock_required = lock_required(snapshot_.state);
}

StorageAccessSnapshot StorageAccessController::snapshot() const noexcept {
  return snapshot_;
}

void StorageAccessController::refresh(
    const StoragePortSnapshot& port_snapshot) noexcept {
  snapshot_.total_bytes = port_snapshot.total_bytes;
  snapshot_.free_bytes = port_snapshot.free_bytes;
  snapshot_.generation = port_snapshot.generation;
  snapshot_.owner = port_snapshot.owner;
  snapshot_.media_present = port_snapshot.media_present ||
                            port_snapshot.mounted ||
                            port_snapshot.owner == StorageOwner::usb_msc;
  snapshot_.filesystem_mounted = port_snapshot.mounted;
  snapshot_.host_seen = port_snapshot.host_seen;
  snapshot_.host_active = port_snapshot.host_active;
  snapshot_.host_ejected = port_snapshot.host_ejected;
  snapshot_.usb_disconnected =
      port_snapshot.usb_stopped && port_snapshot.host_seen;
  snapshot_.suspended = port_snapshot.suspended;
}

void StorageAccessController::dispatch(StoragePortOperation operation) noexcept {
  bool accepted = false;
  switch (operation) {
    case StoragePortOperation::expose:
      accepted = port_.request_expose(operation_token_);
      if (accepted) {
        snapshot_.state = StorageAccessState::exposing;
      }
      break;
    case StoragePortOperation::reclaim:
      accepted = port_.request_reclaim(operation_token_);
      break;
    case StoragePortOperation::remount:
      accepted = port_.request_remount(operation_token_);
      break;
    case StoragePortOperation::none:
      return;
  }
  operation_dispatched_ = accepted;
  if (!accepted) {
    snapshot_.error = StorageAccessError::port_rejected;
    snapshot_.state =
        operation == StoragePortOperation::expose
            ? StorageAccessState::hmi_ready
            : (media_probe_ ? StorageAccessState::unavailable
                            : StorageAccessState::remount_failed);
    pending_operation_ = StoragePortOperation::none;
    media_probe_ = false;
  }
}

void StorageAccessController::handle_completion(
    const StoragePortSnapshot& port_snapshot) noexcept {
  if (!operation_dispatched_ ||
      port_snapshot.operation_token != operation_token_ ||
      port_snapshot.operation != pending_operation_ ||
      port_snapshot.result == StoragePortResult::none ||
      port_snapshot.result == StoragePortResult::in_progress) {
    return;
  }

  const StoragePortOperation completed = pending_operation_;
  pending_operation_ = StoragePortOperation::none;
  operation_dispatched_ = false;
  if (port_snapshot.result != StoragePortResult::success) {
    snapshot_.error = map_error(port_snapshot.result);
    if (completed == StoragePortOperation::remount && media_probe_) {
      snapshot_.state = StorageAccessState::unavailable;
      snapshot_.error = StorageAccessError::no_media;
      media_probe_ = false;
      return;
    }
    if (completed == StoragePortOperation::expose &&
        port_snapshot.mounted && port_snapshot.owner == StorageOwner::hmi) {
      snapshot_.state = StorageAccessState::hmi_ready;
    } else if (completed == StoragePortOperation::expose &&
               port_snapshot.result == StoragePortResult::unavailable) {
      snapshot_.state = StorageAccessState::unavailable;
    } else {
      snapshot_.state = StorageAccessState::remount_failed;
    }
    return;
  }

  snapshot_.error = StorageAccessError::none;
  if (completed == StoragePortOperation::expose) {
    snapshot_.state = StorageAccessState::waiting_for_host;
    disconnect_grace_active_ = false;
    return;
  }
  if (port_snapshot.mounted && port_snapshot.owner == StorageOwner::hmi) {
    snapshot_.state = StorageAccessState::hmi_ready;
    ++snapshot_.rescan_revision;
    disconnect_grace_active_ = false;
    media_probe_ = false;
  } else {
    snapshot_.state = media_probe_ ? StorageAccessState::unavailable
                                   : StorageAccessState::remount_failed;
    snapshot_.error = media_probe_ ? StorageAccessError::no_media
                                   : StorageAccessError::remount_failed;
    media_probe_ = false;
  }
}

void StorageAccessController::begin_reclaim(
    StorageReclaimReason reason) noexcept {
  snapshot_.state = StorageAccessState::reclaiming;
  snapshot_.lock_required = true;
  snapshot_.error = StorageAccessError::none;
  snapshot_.reclaim_reason = reason;
  pending_operation_ = StoragePortOperation::reclaim;
  operation_dispatched_ = false;
  disconnect_grace_active_ = false;
  ++operation_token_;
}

bool StorageAccessController::time_reached(
    std::uint32_t now_ms, std::uint32_t deadline_ms) noexcept {
  return static_cast<std::int32_t>(now_ms - deadline_ms) >= 0;
}

bool StorageAccessController::lock_required(StorageAccessState state) noexcept {
  switch (state) {
    case StorageAccessState::quiescing:
    case StorageAccessState::exposing:
    case StorageAccessState::waiting_for_host:
    case StorageAccessState::pc_active:
    case StorageAccessState::reclaiming:
    case StorageAccessState::remount_failed:
      return true;
    case StorageAccessState::unavailable:
    case StorageAccessState::hmi_ready:
      return false;
  }
  return true;
}

}  // namespace hmi::storage
