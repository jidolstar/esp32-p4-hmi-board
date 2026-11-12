#include <cassert>
#include <utility>

#include "camera/virtual_camera_port.hpp"
#include "camera/virtual_gallery_storage.hpp"
#include "hmi/camera/camera_gallery_controller.hpp"
#include "storage/virtual_storage_access.hpp"

namespace {

using namespace hmi::camera;
using namespace hmi::storage;
using hmi::platform::windows::camera::VirtualCameraPort;
using hmi::platform::windows::camera::VirtualGalleryStorage;
using hmi::platform::windows::storage::VirtualStorageAccess;

StorageAccessSnapshot mounted(std::uint32_t generation = 1U) {
  StorageAccessSnapshot result;
  result.owner = StorageOwner::hmi;
  result.filesystem_mounted = true;
  result.media_present = true;
  result.generation = generation;
  return result;
}

struct Fixture {
  VirtualCameraPort camera;
  VirtualGalleryStorage gallery;
  CameraGalleryController controller{camera, gallery};

  void advance() {
    camera.advance();
    gallery.advance();
    controller.poll();
  }

  void start() {
    controller.update_storage(mounted());
    assert(controller.start());
    assert(!controller.start());
    assert(controller.snapshot().session == CameraSession::starting);
    advance();
    assert(controller.snapshot().session == CameraSession::previewing);
    assert(controller.image() && camera.outstanding() == 1U);
  }

  void save() {
    assert(controller.capture());
    assert(!controller.capture());
    advance();  // capture 완료와 저장 예약
    advance();  // 임시 파일 쓰기
    advance();  // commit과 목록 갱신 예약
    advance();  // 목록 갱신
  }
};

void check_capture_save_view_delete() {
  Fixture f;
  f.start();
  const auto expected_pixel = static_cast<std::uint8_t>(f.controller.image()->frame_id + 1U);
  const auto before = f.controller.snapshot().capture_revision;
  f.save();
  assert(f.controller.snapshot().last_saved_id == 1U);
  assert(f.controller.snapshot().capture_revision == before + 1U);
  assert(f.gallery.exists("IMG_1.jpg") && !f.gallery.exists("IMG_1.jpg.tmp"));
  f.save();
  assert(f.controller.page().count == 2U);
  assert(f.controller.page().items[0].id == 2U && f.controller.page().items[1].id == 1U);
  assert(!f.controller.open(17U));
  assert(f.gallery.request_scan(999U, 0U));
  assert(!f.controller.open(1U));
  assert(f.controller.snapshot().storage_error == GalleryError::lease_busy);
  f.advance();
  assert(f.camera.running() && f.controller.snapshot().session == CameraSession::previewing);

  assert(f.controller.open(1U));
  assert(!f.camera.running() && f.camera.outstanding() == 0U && !f.controller.image());
  assert(!f.controller.back_to_preview());
  f.advance();
  assert(f.controller.snapshot().session == CameraSession::viewing);
  assert(f.gallery.has_viewer_lease() && f.controller.image()->pixels[0] == expected_pixel);
  assert(f.controller.image()->bytes == VirtualCameraPort::Pixels{}.size());
  assert(!f.controller.delete_selected(2U));
  assert(f.controller.delete_selected(1U));
  assert(!f.gallery.has_viewer_lease());
  assert(!f.controller.delete_selected(1U));
  f.advance();
  assert(!f.gallery.exists("IMG_1.jpg") && f.gallery.exists("IMG_2.jpg"));
  f.advance();
  f.advance();
  assert(f.controller.snapshot().selected_id == 2U && f.controller.image());
  assert(f.controller.page().count == 1U);

  assert(f.controller.delete_selected(2U));
  f.advance();
  f.advance();
  assert(f.controller.page().count == 0U && !f.controller.image());
  assert(f.controller.snapshot().selected_id == 0U);
  assert(f.controller.snapshot().last_saved_id == 0U);
  assert(f.controller.snapshot().session == CameraSession::starting);
  f.advance();
  assert(f.controller.snapshot().session == CameraSession::previewing);
  assert(f.camera.running() && f.controller.image() && !f.gallery.has_viewer_lease());
  f.controller.stop();
  assert(!f.camera.running() && f.camera.outstanding() == 0U);
  assert(!f.gallery.busy() && f.controller.snapshot().session == CameraSession::idle);
  f.start();
  assert(f.controller.page().count == 0U);
}

void check_partial_write_and_collision() {
  Fixture f;
  f.start();
  f.gallery.fail_next(GalleryError::write_failed);
  assert(f.controller.capture());
  f.advance();
  assert(f.camera.outstanding() == 1U && !f.controller.refresh());
  f.advance();
  assert(f.camera.outstanding() == 0U && f.gallery.exists("IMG_1.jpg.tmp"));
  assert(!f.gallery.exists("IMG_1.jpg") && f.controller.page().count == 0U);
  f.advance();
  assert(f.controller.snapshot().storage_error == GalleryError::write_failed);
  assert(f.controller.snapshot().camera_error == CameraError::none);
  assert(f.controller.refresh());
  f.advance();
  assert(!f.gallery.exists("IMG_1.jpg.tmp") && f.controller.page().count == 0U);

  assert(f.controller.capture());
  f.advance();
  f.advance();
  assert(f.gallery.exists("IMG_2.jpg.tmp"));
  assert(f.gallery.seed("IMG_2.jpg"));  // commit 직전 같은 최종 이름이 생긴다.
  f.advance();
  assert(f.controller.snapshot().storage_error == GalleryError::write_failed);
  assert(f.controller.snapshot().last_saved_id == 0U);
  assert(f.controller.refresh());
  f.advance();
  assert(f.controller.page().count == 1U && !f.gallery.exists("IMG_2.jpg.tmp"));
  assert(f.controller.open(2U));
  f.advance();
  assert(f.controller.image()->pixels[0] == 0x42U);  // 기존 파일의 fixture 데이터가 유지된다.
  assert(f.controller.back_to_preview());
  f.advance();
  f.save();
  assert(f.controller.snapshot().last_saved_id == 3U);
}

void check_errors_and_untrusted_names() {
  Fixture f;
  f.camera.fail_next(CameraError::probe_failed);
  assert(f.controller.start());
  f.advance();
  assert(f.controller.snapshot().session == CameraSession::failed);
  assert(f.controller.snapshot().camera_error == CameraError::probe_failed);
  assert(f.controller.start());
  f.advance();
  assert(f.controller.image() && !f.controller.capture());
  assert(f.controller.snapshot().storage_error == GalleryError::not_ready);

  using FileKind = VirtualGalleryStorage::FileKind;
  assert(f.gallery.seed("IMG_9.jpg", FileKind::corrupt));
  assert(f.gallery.seed("../IMG_7.jpg"));
  assert(f.gallery.seed("IMG_10.jpg", FileKind::non_regular));
  assert(f.gallery.seed("IMG_11.jpg.tmp"));
  assert(f.gallery.seed("../IMG_12.jpg.tmp"));
  assert(f.gallery.seed("IMG_13.jpg.tmp", FileKind::non_regular));
  f.controller.update_storage(mounted());
  f.advance();
  assert(f.controller.page().count == 1U && f.controller.page().items[0].id == 9U);
  assert(!f.gallery.exists("IMG_11.jpg.tmp") && f.gallery.exists("../IMG_12.jpg.tmp"));
  assert(f.gallery.exists("IMG_13.jpg.tmp"));
  assert(!f.controller.open(7U) && !f.controller.open(10U));
  assert(f.controller.open(9U));
  f.advance();
  assert(f.controller.snapshot().storage_error == GalleryError::invalid_file);
  assert(!f.controller.image() && f.controller.snapshot().selected_id == 9U);
  f.gallery.fail_next(GalleryError::delete_failed);
  assert(f.controller.delete_selected(9U));
  f.advance();
  assert(f.controller.snapshot().storage_error == GalleryError::delete_failed);
  assert(f.controller.snapshot().session == CameraSession::viewing);
  assert(f.gallery.exists("IMG_9.jpg") && f.controller.delete_selected(9U));
  f.advance();
  f.advance();
  assert(!f.gallery.exists("IMG_9.jpg") && f.gallery.exists("IMG_10.jpg"));
  assert(f.gallery.exists("../IMG_7.jpg") && f.controller.page().count == 0U);
  assert(f.controller.snapshot().session == CameraSession::starting);
  f.advance();
  assert(f.controller.snapshot().session == CameraSession::previewing);

  f.camera.fail_next(CameraError::frame_timeout);
  assert(f.controller.capture());
  f.advance();
  assert(f.controller.snapshot().camera_error == CameraError::frame_timeout);
  assert(f.controller.snapshot().storage_error == GalleryError::none);
  f.gallery.fail_next(GalleryError::full);
  assert(f.controller.capture());
  f.advance();
  f.advance();
  assert(f.controller.snapshot().storage_error == GalleryError::full);
  assert(f.controller.snapshot().camera_error == CameraError::none);
  assert(f.camera.outstanding() == 1U);

  assert(f.gallery.request_scan(999U, 0U));
  assert(f.controller.capture());
  f.advance();
  assert(f.controller.snapshot().storage_error == GalleryError::lease_busy);
  assert(f.controller.snapshot().session == CameraSession::previewing && f.camera.outstanding() == 1U);
}

void check_msc_and_late_results() {
  Fixture f;
  VirtualStorageAccess sd;
  StorageAccessController s05(sd);
  s05.tick(0U);
  f.controller.update_storage(s05.snapshot());
  assert(f.controller.start());
  f.advance();
  assert(f.controller.capture());
  const auto old_token = f.controller.snapshot().capture_token;
  f.advance();
  assert(f.gallery.busy() && f.camera.outstanding() == 1U);
  assert(s05.request_pc_access(1U));
  assert(s05.snapshot().owner == StorageOwner::hmi && s05.snapshot().lock_required);
  f.controller.update_storage(s05.snapshot());
  assert(f.camera.outstanding() == 0U && !f.camera.running() && !f.gallery.busy());
  assert(!f.gallery.exists("IMG_1.jpg") && !f.controller.capture());
  s05.tick(2U);
  assert(sd.complete_pending());
  s05.tick(3U);
  f.controller.update_storage(s05.snapshot());
  assert(s05.snapshot().owner == StorageOwner::usb_msc);
  assert(f.controller.snapshot().storage_error == GalleryError::pc_owner);
  assert(!f.controller.start() && !f.controller.refresh());

  assert(s05.request_hmi_access(4U, StorageReclaimReason::explicit_request));
  s05.tick(5U);
  assert(sd.complete_pending());
  s05.tick(6U);
  f.controller.update_storage(s05.snapshot());
  assert(f.controller.start());
  assert(f.camera.inject_completion({old_token, CameraError::frame_timeout, {}}));
  GalleryCompletion stale;
  stale.token = old_token;
  stale.error = GalleryError::invalid_file;
  assert(f.gallery.inject_completion(std::move(stale)));
  f.controller.poll();
  assert(f.controller.snapshot().session == CameraSession::starting);
  assert(f.controller.snapshot().camera_error == CameraError::none);
  assert(f.controller.snapshot().storage_busy && f.controller.snapshot().storage_error == GalleryError::none);
  f.advance();
  f.save();
  assert(f.controller.open(f.controller.snapshot().last_saved_id));
  f.advance();
  assert(f.gallery.has_viewer_lease());
  f.controller.update_storage({});  // 매체를 빼면 뷰어를 닫고 미리보기만 복구한다.
  f.advance();
  assert(f.camera.running() && !f.gallery.has_viewer_lease());
  assert(f.controller.snapshot().storage_error == GalleryError::not_ready);
}

void check_destruction_and_paging() {
  VirtualCameraPort camera;
  VirtualGalleryStorage gallery;
  for (PhotoId id = 1U; id <= 10U; ++id) {
    assert(gallery.seed(photo_filename(id).data()));
  }
  {
    CameraGalleryController controller(camera, gallery);
    controller.update_storage(mounted());
    assert(controller.start());
    camera.advance();
    gallery.advance();
    controller.poll();
    assert(controller.page().count == 8U && controller.snapshot().item_count == 10U);
    assert(controller.refresh(1U));
    gallery.advance();
    controller.poll();
    assert(controller.page().count == 2U && controller.page().items[0].id == 2U);
    assert(!controller.open(10U) && controller.open(2U));
    gallery.advance();
    controller.poll();
    assert(gallery.has_viewer_lease());
  }
  assert(!camera.running() && camera.outstanding() == 0U && !gallery.busy());
  assert(!gallery.has_viewer_lease());
}

}  // namespace

int main() {
  check_capture_save_view_delete();
  check_partial_write_and_collision();
  check_errors_and_untrusted_names();
  check_msc_and_late_results();
  check_destruction_and_paging();
}
