#include "hmi/camera/camera_gallery_controller.hpp"

#include <algorithm>
#include <utility>

namespace hmi::camera {

std::uint32_t CameraGalleryController::next_token() noexcept {
  if (++token_ == 0U) {
    ++token_;
  }
  return token_;
}

bool CameraGalleryController::start_preview() noexcept {
  camera_token_ = next_token();
  if (!camera_.request_start(camera_token_)) {
    snapshot_.camera_error = CameraError::stream_failed;
    snapshot_.session = CameraSession::failed;
    return false;
  }
  camera_work_ = CameraWork::start;
  snapshot_.camera_error = CameraError::none;
  snapshot_.session = CameraSession::starting;
  return true;
}

bool CameraGalleryController::start() noexcept {
  if (storage_locked_ || (active_ && snapshot_.session != CameraSession::failed)) {
    return false;
  }
  const bool entering = !active_;
  active_ = true;
  const bool accepted = start_preview();
  if (entering && snapshot_.storage_ready) {
    scan(0U);
  }
  return accepted;
}

void CameraGalleryController::finish_capture() noexcept {
  ++snapshot_.capture_revision;
  snapshot_.session = CameraSession::previewing;
}

void CameraGalleryController::cancel_gallery() noexcept {
  image_.reset();
  storage_.cancel_and_drain();
  gallery_work_ = GalleryWork::none;
  snapshot_.storage_busy = false;
  snapshot_.selected_id = 0U;
  select_after_scan_ = false;
}

void CameraGalleryController::clear_page() noexcept {
  page_ = {};
  snapshot_.page_index = 0U;
  snapshot_.item_count = 0U;
  ++snapshot_.gallery_revision;
}

void CameraGalleryController::stop() noexcept {
  if (!active_) {
    return;
  }
  // 저장 작업이 카메라 프레임을 쥐고 있을 수 있으므로 파일 작업부터 정리한다.
  cancel_gallery();
  camera_.stop();
  camera_work_ = CameraWork::none;
  if (snapshot_.session == CameraSession::capturing) {
    finish_capture();
  }
  active_ = false;
  snapshot_.session = CameraSession::idle;
  snapshot_.camera_error = CameraError::none;
  snapshot_.last_saved_id = 0U;
  clear_page();
}

void CameraGalleryController::update_storage(const hmi::storage::StorageAccessSnapshot& access) noexcept {
  using hmi::storage::StorageOwner;
  const bool locked = access.lock_required || access.owner == StorageOwner::usb_msc;
  const bool ready = !locked && access.owner == StorageOwner::hmi && access.filesystem_mounted;
  if (locked == storage_locked_ && ready == snapshot_.storage_ready &&
      storage_generation_ == access.generation && rescan_revision_ == access.rescan_revision) {
    return;
  }
  storage_locked_ = locked;
  storage_generation_ = access.generation;
  rescan_revision_ = access.rescan_revision;
  snapshot_.storage_ready = ready;
  snapshot_.storage_error = locked ? GalleryError::pc_owner
                                   : (ready ? GalleryError::none : GalleryError::not_ready);
  snapshot_.last_saved_id = 0U;

  if (locked) {
    stop();
    return;
  }
  if (!active_) {
    return;
  }

  const auto previous = snapshot_.session;
  cancel_gallery();
  clear_page();
  if (previous == CameraSession::capturing || previous == CameraSession::viewing) {
    if (previous == CameraSession::capturing) {
      finish_capture();
    }
    camera_.stop();
    camera_work_ = CameraWork::none;
    start_preview();
  }
  if (ready) {
    scan(0U);
  }
}

bool CameraGalleryController::gallery_available() const noexcept {
  return active_ && snapshot_.storage_ready && gallery_work_ == GalleryWork::none;
}

bool CameraGalleryController::scan(std::size_t page_index) noexcept {
  gallery_token_ = next_token();
  if (!storage_.request_scan(gallery_token_, page_index)) {
    snapshot_.storage_error = GalleryError::lease_busy;
    select_after_scan_ = false;
    return false;
  }
  gallery_work_ = GalleryWork::scan;
  snapshot_.storage_busy = true;
  return true;
}

bool CameraGalleryController::refresh(std::size_t page_index) noexcept {
  if (!gallery_available() || snapshot_.session == CameraSession::capturing ||
      page_index >= (kGalleryIndexCapacity + kGalleryPageCapacity - 1U) / kGalleryPageCapacity) {
    return false;
  }
  return scan(page_index);
}

bool CameraGalleryController::capture() noexcept {
  if (!gallery_available() || snapshot_.session != CameraSession::previewing) {
    return false;
  }
  release_preview();
  camera_token_ = next_token();
  if (!camera_.request_capture(camera_token_)) {
    snapshot_.camera_error = CameraError::stream_failed;
    return false;
  }
  camera_work_ = CameraWork::capture;
  snapshot_.camera_error = CameraError::none;
  snapshot_.storage_error = GalleryError::none;
  snapshot_.last_saved_id = 0U;
  snapshot_.capture_token = camera_token_;
  snapshot_.session = CameraSession::capturing;
  return true;
}

bool CameraGalleryController::load(PhotoId id) noexcept {
  gallery_token_ = next_token();
  if (!storage_.request_open(gallery_token_, id)) {
    snapshot_.storage_error = GalleryError::lease_busy;
    return false;
  }
  gallery_work_ = GalleryWork::open;
  snapshot_.storage_busy = true;
  snapshot_.storage_error = GalleryError::none;
  snapshot_.selected_id = id;
  return true;
}

bool CameraGalleryController::open(PhotoId id) noexcept {
  const auto session = snapshot_.session;
  if (!gallery_available() || id == 0U ||
      (session != CameraSession::previewing && session != CameraSession::viewing &&
       session != CameraSession::failed)) {
    return false;
  }
  const auto end = page_.items.begin() + page_.count;
  if (std::find_if(page_.items.begin(), end, [id](const auto& item) { return item.id == id; }) == end) {
    return false;
  }
  // 파일 worker가 즉시 실행돼도 이전 뷰와 디코드 버퍼가 동시에 생기지 않게 한다.
  image_.reset();
  if (session == CameraSession::previewing) {
    camera_.stop();
  }
  if (!load(id)) {
    if (session == CameraSession::previewing) {
      start_preview();
    }
    return false;
  }
  snapshot_.session = CameraSession::viewing;
  return true;
}

bool CameraGalleryController::back_to_preview() noexcept {
  if (snapshot_.session != CameraSession::viewing || gallery_work_ != GalleryWork::none) {
    return false;
  }
  image_.reset();
  snapshot_.selected_id = 0U;
  snapshot_.storage_error = GalleryError::none;
  return start_preview();
}

bool CameraGalleryController::delete_selected(PhotoId confirmed_id) noexcept {
  if (!gallery_available() || snapshot_.session != CameraSession::viewing || confirmed_id == 0U ||
      confirmed_id != snapshot_.selected_id) {
    return false;
  }
  // 파일 작업 예약 전에 뷰어 lease를 돌려준다. 오류 시 같은 ID를 다시 열 수 있다.
  image_.reset();
  gallery_token_ = next_token();
  if (!storage_.request_delete(gallery_token_, confirmed_id)) {
    snapshot_.storage_error = GalleryError::lease_busy;
    return false;
  }
  gallery_work_ = GalleryWork::erase;
  snapshot_.storage_busy = true;
  snapshot_.storage_error = GalleryError::none;
  const auto end = page_.items.begin() + page_.count;
  const auto selected = std::find_if(page_.items.begin(), end, [confirmed_id](const auto& item) {
    return item.id == confirmed_id;
  });
  deleted_position_ = static_cast<std::size_t>(selected - page_.items.begin());
  return true;
}

void CameraGalleryController::handle_camera(CameraCompletion& result, CaptureFrameCallback on_capture,
                                            void* context) noexcept {
  if (camera_work_ == CameraWork::none || result.token != camera_token_) {
    return;
  }
  const auto work = std::exchange(camera_work_, CameraWork::none);
  snapshot_.camera_error = result.error;
  if (work == CameraWork::start) {
    snapshot_.session = result.error == CameraError::none ? CameraSession::previewing : CameraSession::failed;
    return;
  }
  if (result.error != CameraError::none || !result.frame) {
    if (result.error == CameraError::none) {
      snapshot_.camera_error = CameraError::stream_failed;
    }
    finish_capture();
    return;
  }
  if (on_capture) {
    on_capture(context, result.frame.view());
  }
  gallery_token_ = next_token();
  if (!storage_.request_save(gallery_token_, std::move(result.frame))) {
    snapshot_.storage_error = GalleryError::lease_busy;
    finish_capture();
    return;
  }
  gallery_work_ = GalleryWork::save;
  snapshot_.storage_busy = true;
}

void CameraGalleryController::handle_gallery(GalleryCompletion& result) noexcept {
  if (gallery_work_ == GalleryWork::none || result.token != gallery_token_) {
    return;
  }
  const auto work = std::exchange(gallery_work_, GalleryWork::none);
  snapshot_.storage_busy = false;
  snapshot_.storage_error = result.error;
  if (work == GalleryWork::save) {
    finish_capture();
  }
  if (result.error != GalleryError::none) {
    select_after_scan_ = false;
    return;
  }

  switch (work) {
    case GalleryWork::scan: {
      if (result.total > kGalleryIndexCapacity || result.page.count > kGalleryPageCapacity ||
          result.page_index > (result.total == 0U ? 0U : (result.total - 1U) / kGalleryPageCapacity) ||
          result.page.count != std::min(kGalleryPageCapacity, result.total - result.page_index * kGalleryPageCapacity)) {
        select_after_scan_ = false;
        snapshot_.storage_error = GalleryError::invalid_file;
        return;
      }
      const bool previous_page = result.page_index < snapshot_.page_index;
      page_ = result.page;
      snapshot_.page_index = static_cast<std::uint8_t>(result.page_index);
      snapshot_.item_count = static_cast<std::uint8_t>(result.total);
      ++snapshot_.gallery_revision;
      if (!std::exchange(select_after_scan_, false)) {
        break;
      }
      if (page_.count == 0U) {
        // 삭제 직후 재스캔으로 전체 목록이 비었음을 확인한 뒤 카메라를 다시 시작한다.
        snapshot_.last_saved_id = 0U;
        back_to_preview();
      } else {
        const auto position = previous_page ? page_.count - 1U : std::min(deleted_position_, page_.count - 1U);
        load(page_.items[position].id);
      }
      break;
    }
    case GalleryWork::save:
      if (result.saved_id == 0U) {
        snapshot_.storage_error = GalleryError::write_failed;
        return;
      }
      snapshot_.last_saved_id = result.saved_id;
      scan(0U);
      break;
    case GalleryWork::open:
      image_ = std::move(result.image);
      if (!image_) {
        snapshot_.storage_error = GalleryError::invalid_file;
      }
      break;
    case GalleryWork::erase:
      snapshot_.selected_id = 0U;
      if (deleted_position_ < page_.count) {
        const auto first = page_.items.begin() + deleted_position_;
        std::move(first + 1, page_.items.begin() + page_.count, first);
        page_.items[--page_.count] = {};
        --snapshot_.item_count;
        ++snapshot_.gallery_revision;
      }
      select_after_scan_ = true;
      scan(snapshot_.page_index);
      break;
    case GalleryWork::none:
      break;
  }
}

void CameraGalleryController::poll(CaptureFrameCallback on_capture, void* context) noexcept {
  if (!active_) {
    return;
  }
  CameraCompletion camera_result;
  if (camera_.take_completion(camera_result)) {
    handle_camera(camera_result, on_capture, context);
  }
  GalleryCompletion gallery_result;
  if (storage_.take_completion(gallery_result)) {
    handle_gallery(gallery_result);
  }
  if (snapshot_.session == CameraSession::previewing) {
    auto frame = camera_.acquire_preview();
    if (frame) {
      image_ = std::move(frame);
    }
  }
}

const CameraFrameView* CameraGalleryController::image() const noexcept {
  return image_ ? &image_.view() : nullptr;
}

void CameraGalleryController::release_preview() noexcept {
  if (snapshot_.session == CameraSession::previewing) {
    image_.reset();
  }
}

}  // namespace hmi::camera
