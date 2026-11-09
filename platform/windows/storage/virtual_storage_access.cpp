#include "storage/virtual_storage_access.hpp"

namespace hmi::platform::windows::storage {

VirtualStorageAccess::VirtualStorageAccess() noexcept {
  snapshot_.total_bytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
  snapshot_.free_bytes = 24ULL * 1024ULL * 1024ULL * 1024ULL;
  snapshot_.generation = 1U;
  snapshot_.owner = ::hmi::storage::StorageOwner::hmi;
  snapshot_.mounted = true;
  snapshot_.media_present = true;
}

bool VirtualStorageAccess::request_expose(std::uint32_t token) noexcept {
  return request(::hmi::storage::StoragePortOperation::expose, token);
}

bool VirtualStorageAccess::request_reclaim(std::uint32_t token) noexcept {
  return request(::hmi::storage::StoragePortOperation::reclaim, token);
}

bool VirtualStorageAccess::request_remount(std::uint32_t token) noexcept {
  return request(::hmi::storage::StoragePortOperation::remount, token);
}

bool VirtualStorageAccess::request(
    ::hmi::storage::StoragePortOperation operation,
    std::uint32_t token) noexcept {
  if (pending_ || operation == ::hmi::storage::StoragePortOperation::none) {
    return false;
  }
  pending_ = true;
  pending_operation_ = operation;
  pending_token_ = token;
  snapshot_.operation = operation;
  snapshot_.operation_token = token;
  snapshot_.result = ::hmi::storage::StoragePortResult::in_progress;
  return true;
}

void VirtualStorageAccess::tick(std::uint32_t) noexcept {
  if (auto_complete_) {
    complete_pending();
  }
}

::hmi::storage::StoragePortSnapshot VirtualStorageAccess::snapshot()
    const noexcept {
  return snapshot_;
}

bool VirtualStorageAccess::complete_pending() noexcept {
  if (!pending_) {
    return false;
  }
  const auto operation = pending_operation_;
  const auto token = pending_token_;
  const auto result = next_result_;
  next_result_ = ::hmi::storage::StoragePortResult::success;
  pending_ = false;
  pending_operation_ = ::hmi::storage::StoragePortOperation::none;
  pending_token_ = 0U;
  apply_completion(operation, result);
  snapshot_.operation = operation;
  snapshot_.operation_token = token;
  snapshot_.result = result;
  ++snapshot_.completion_revision;
  return true;
}

void VirtualStorageAccess::inject_completion(
    ::hmi::storage::StoragePortOperation operation, std::uint32_t token,
    ::hmi::storage::StoragePortResult result) noexcept {
  snapshot_.operation = operation;
  snapshot_.operation_token = token;
  snapshot_.result = result;
  ++snapshot_.completion_revision;
}

void VirtualStorageAccess::apply_completion(
    ::hmi::storage::StoragePortOperation operation,
    ::hmi::storage::StoragePortResult result) noexcept {
  if (operation == ::hmi::storage::StoragePortOperation::expose) {
    if (result == ::hmi::storage::StoragePortResult::success) {
      snapshot_.owner = ::hmi::storage::StorageOwner::usb_msc;
      snapshot_.mounted = false;
      snapshot_.media_present = true;
      snapshot_.host_seen = false;
      snapshot_.host_active = false;
      snapshot_.host_ejected = false;
      snapshot_.usb_stopped = false;
      snapshot_.suspended = false;
      ++snapshot_.event_revision;
    }
    return;
  }
  if (result == ::hmi::storage::StoragePortResult::success) {
    snapshot_.owner = ::hmi::storage::StorageOwner::hmi;
    snapshot_.mounted = true;
    snapshot_.media_present = true;
    snapshot_.host_seen = false;
    snapshot_.host_active = false;
    snapshot_.host_ejected = false;
    snapshot_.usb_started = false;
    snapshot_.usb_stopped = false;
    snapshot_.suspended = false;
    ++snapshot_.generation;
    ++snapshot_.event_revision;
  } else if (result ==
             ::hmi::storage::StoragePortResult::callback_drain_timeout) {
    snapshot_.owner = ::hmi::storage::StorageOwner::usb_msc;
    snapshot_.mounted = false;
    snapshot_.media_present = false;
  } else if (result ==
             ::hmi::storage::StoragePortResult::remount_failed) {
    snapshot_.owner = ::hmi::storage::StorageOwner::none;
    snapshot_.mounted = false;
    snapshot_.media_present = true;
  }
}

void VirtualStorageAccess::set_media_present(bool present) noexcept {
  snapshot_.media_present = present;
  snapshot_.mounted = present;
  snapshot_.owner = present ? ::hmi::storage::StorageOwner::hmi
                            : ::hmi::storage::StorageOwner::none;
  if (present) {
    ++snapshot_.generation;
  }
  ++snapshot_.event_revision;
}

void VirtualStorageAccess::host_read() noexcept {
  if (snapshot_.owner != ::hmi::storage::StorageOwner::usb_msc) {
    return;
  }
  snapshot_.host_seen = true;
  snapshot_.host_active = true;
  ++snapshot_.event_revision;
}

void VirtualStorageAccess::host_eject() noexcept {
  if (snapshot_.owner != ::hmi::storage::StorageOwner::usb_msc) {
    return;
  }
  snapshot_.host_ejected = true;
  snapshot_.host_active = false;
  ++snapshot_.event_revision;
}

void VirtualStorageAccess::usb_unplug() noexcept {
  if (snapshot_.owner != ::hmi::storage::StorageOwner::usb_msc) {
    return;
  }
  snapshot_.usb_stopped = true;
  snapshot_.usb_started = false;
  snapshot_.host_active = false;
  snapshot_.suspended = false;
  ++snapshot_.event_revision;
}

void VirtualStorageAccess::usb_suspend() noexcept {
  snapshot_.suspended = true;
  ++snapshot_.event_revision;
}

void VirtualStorageAccess::usb_resume() noexcept {
  snapshot_.suspended = false;
  ++snapshot_.event_revision;
}

}  // namespace hmi::platform::windows::storage
