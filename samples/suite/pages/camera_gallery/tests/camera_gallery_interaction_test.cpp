#include <cassert>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>

#include "app.hpp"
#include "camera/virtual_camera_port.hpp"
#include "camera/virtual_gallery_storage.hpp"
#include "hmi/camera/camera_gallery_controller.hpp"
#include "sdl_lvgl_port.hpp"
#include "storage/virtual_storage_access.hpp"

namespace {
lv_obj_t* visible_label(lv_obj_t* widget, const char* text) {
  if (lv_obj_has_flag(widget, LV_OBJ_FLAG_HIDDEN)) return nullptr;
  if (lv_obj_check_type(widget, &lv_label_class) && std::strcmp(lv_label_get_text(widget), text) == 0) {
    return widget;
  }
  for (std::uint32_t index = 0; index < lv_obj_get_child_cnt(widget); ++index) {
    if (auto* found = visible_label(lv_obj_get_child(widget, index), text)) return found;
  }
  return nullptr;
}

unsigned visible_thumbnails(lv_obj_t* widget) {
  if (lv_obj_has_flag(widget, LV_OBJ_FLAG_HIDDEN)) return 0U;
  unsigned count = lv_obj_check_type(widget, &lv_img_class) && lv_obj_get_width(widget) == 72;
  for (std::uint32_t index = 0; index < lv_obj_get_child_cnt(widget); ++index) {
    count += visible_thumbnails(lv_obj_get_child(widget, index));
  }
  return count;
}

void verify_confirmation_layout(const char* title, const char* cancel, const char* confirm) {
  auto* screen = lv_scr_act();
  lv_obj_update_layout(screen);
  auto* question = visible_label(screen, title);
  assert(question);
  auto* panel = lv_obj_get_parent(question);
  auto* scrim = lv_obj_get_parent(panel);
  assert(lv_obj_get_parent(scrim) == screen);
  assert(lv_obj_get_child(screen, -1) == scrim);
  assert(lv_obj_has_flag(scrim, LV_OBJ_FLAG_CLICKABLE));
  lv_area_t bounds{}, shaded{}, card{}, left{}, right{}, heading{};
  lv_obj_get_coords(screen, &bounds);
  lv_obj_get_coords(scrim, &shaded);
  lv_obj_get_coords(panel, &card);
  lv_obj_get_coords(question, &heading);
  assert(shaded.x1 == bounds.x1 && shaded.y1 == bounds.y1 &&
         shaded.x2 == bounds.x2 && shaded.y2 == bounds.y2);
  assert(std::abs(card.x1 + card.x2 - bounds.x1 - bounds.x2) <= 1);
  assert(std::abs(card.y1 + card.y2 - bounds.y1 - bounds.y2) <= 1);
  lv_point_t header{60, 40};
  assert(lv_indev_search_obj(screen, &header) == scrim);
  auto* cancel_label = visible_label(panel, cancel);
  auto* confirm_label = visible_label(panel, confirm);
  assert(cancel_label && confirm_label);
  lv_obj_get_coords(lv_obj_get_parent(cancel_label), &left);
  lv_obj_get_coords(lv_obj_get_parent(confirm_label), &right);
  assert(lv_obj_get_style_text_align(question, 0) == LV_TEXT_ALIGN_CENTER);
  assert(left.x2 < right.x1 && left.y1 == right.y1 && left.y2 == right.y2);
  assert(std::abs(lv_area_get_width(&left) - lv_area_get_width(&right)) <= 1);
  assert(lv_area_get_height(&left) >= 48 && heading.y2 < left.y1);
  assert(left.x1 > card.x1 && right.x2 < card.x2 && left.y2 < card.y2);
}

void press(const char* text, bool last = false) {
  auto* label = visible_label(lv_scr_act(), text);
  assert(label);
  if (last) {
    // 확인창이 열리면 같은 문구를 가진 뒤쪽 화면 대신 scrim의 버튼을 누른다.
    auto* question = visible_label(lv_scr_act(), "Delete this photo?");
    assert(question);
    label = visible_label(lv_obj_get_parent(question), text);
    assert(label);
  }
  auto* button = lv_obj_get_parent(label);
  assert(!lv_obj_has_state(button, LV_STATE_DISABLED));
  lv_event_send(button, LV_EVENT_SHORT_CLICKED, nullptr);
}
}  // namespace

int main(int argc, char** argv) {
  using hmi::camera::CameraSession;
  using hmi::samples::suite::SuiteView;

  lv_init();
  hmi::platform::windows::SdlLvglPort display;
  assert(display.begin("camera gallery interaction", false));
  hmi::platform::windows::storage::VirtualStorageAccess sd_port;
  hmi::storage::StorageAccessController sd(sd_port);
  sd.tick(0U);
  hmi::platform::windows::camera::VirtualCameraPort camera;
  hmi::platform::windows::camera::VirtualGalleryStorage files;
  hmi::camera::CameraGalleryController gallery(camera, files);
  hmi::profiles::AppContext context;
  context.host_name = "windows-test";
  context.storage_access = &sd;
  context.camera_controller = &gallery;
  auto app = std::make_unique<hmi::samples::suite::SampleSuiteApp>();
  assert(app->create(context));
  const auto advance = [&] {
    for (int step = 0; step < 6; ++step) {
      camera.advance();
      files.advance();
      lv_tick_inc(65);
      lv_timer_handler();
    }
    lv_obj_update_layout(lv_scr_act());
  };
  const auto screenshot = [&](const char* suffix) {
    if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
      lv_obj_invalidate(lv_scr_act());
      lv_refr_now(nullptr);
      assert(display.save_screenshot((std::string(argv[2]) + suffix).c_str()));
    }
  };

  assert(app->request_page("camera-gallery"));
  assert(app->flush_pending_transition());
  advance();
  assert(gallery.snapshot().session == CameraSession::previewing);
  assert(visible_label(lv_scr_act(), "저장된 사진이 없습니다."));
  auto* capture_label = visible_label(lv_scr_act(), "촬영");
  assert(capture_label);
  lv_area_t bounds{};
  lv_obj_get_coords(lv_obj_get_parent(capture_label), &bounds);
  assert(bounds.x1 >= 0 && bounds.y1 >= 0 && bounds.x2 < 1024 && bounds.y2 < 600);
  screenshot(".ko-empty.bmp");

  press("촬영");
  press("촬영");
  advance();
  assert(gallery.snapshot().item_count == 1U);
  assert(gallery.snapshot().capture_revision == 1U);
  assert(files.exists("IMG_1.jpg"));
  auto* saved_name = visible_label(lv_scr_act(), "IMG_1.jpg");
  assert(saved_name && visible_thumbnails(lv_obj_get_parent(saved_name)) == 1U);
  assert(gallery.snapshot().session == CameraSession::previewing && !files.has_viewer_lease());
  press("IMG_1.jpg");
  advance();
  assert(gallery.snapshot().session == CameraSession::viewing);
  assert(gallery.snapshot().selected_id == 1U && files.has_viewer_lease());

  press("삭제");
  verify_confirmation_layout("이 사진을 삭제할까요?", "취소", "삭제");
  screenshot(".ko-confirm.bmp");
  press("취소");
  assert(files.exists("IMG_1.jpg"));

  press("영어");
  assert(app->flush_pending_transition());
  assert(app->request_theme_toggle());
  assert(app->flush_pending_transition());
  advance();
  assert(gallery.snapshot().selected_id == 1U && files.has_viewer_lease());
  assert(visible_label(lv_scr_act(), "Saved photos"));
  assert(visible_thumbnails(lv_scr_act()) == 1U);
  screenshot(".en-viewer.bmp");

  press("Delete");
  press("Cancel");
  assert(files.exists("IMG_1.jpg"));
  press("Delete");
  verify_confirmation_layout("Delete this photo?", "Cancel", "Delete");
  screenshot(".en-confirm.bmp");
  press("Delete", true);
  advance();
  assert(!files.exists("IMG_1.jpg") && gallery.snapshot().item_count == 0U);
  assert(gallery.snapshot().session == CameraSession::previewing);
  assert(camera.running() && visible_label(lv_scr_act(), "No saved photos."));
  assert(!visible_label(lv_scr_act(), "Photo saved."));
  files.fail_next(hmi::camera::GalleryError::full);
  press("Capture");
  advance();
  assert(visible_label(lv_scr_act(), "SD card is full."));
  assert(gallery.snapshot().item_count == 0U);
  assert(visible_thumbnails(lv_scr_act()) == 0U);
  press("Back");
  assert(app->flush_pending_transition());
  assert(app->snapshot().view == SuiteView::kDashboard);
  assert(!camera.running() && camera.outstanding() == 0U && !files.has_viewer_lease());

  assert(files.seed("IMG_9.jpg", decltype(files)::FileKind::corrupt));
  assert(app->request_page("camera-gallery"));
  assert(app->flush_pending_transition());
  advance();
  press("IMG_9.jpg");
  advance();
  assert(visible_label(lv_scr_act(), "Could not open the photo."));
  assert(!files.has_viewer_lease());
  press("Back");
  advance();
  assert(gallery.snapshot().session == CameraSession::previewing);
  for (const char* filename : {"IMG_10.jpg", "IMG_11.jpg", "IMG_12.jpg", "IMG_13.jpg", "IMG_14.jpg"}) {
    assert(files.seed(filename));
  }
  assert(gallery.refresh());
  advance();
  for (const char* filename : {"IMG_10.jpg", "IMG_11.jpg", "IMG_12.jpg", "IMG_13.jpg", "IMG_14.jpg"}) {
    press(filename);
    advance();
    assert(gallery.snapshot().session == CameraSession::viewing && files.has_viewer_lease());
    press("Preview");
    advance();
  }
  assert(visible_thumbnails(lv_scr_act()) == 4U);
  press("Capture");
  advance();
  const auto latest = hmi::camera::photo_filename(gallery.snapshot().last_saved_id);
  auto* latest_name = visible_label(lv_scr_act(), latest.data());
  assert(latest_name && visible_thumbnails(lv_obj_get_parent(latest_name)) == 1U);
  assert(visible_thumbnails(lv_scr_act()) == 4U && !files.has_viewer_lease());
  sd_port.set_media_present(false);
  sd.tick(lv_tick_get());
  advance();
  assert(visible_label(lv_scr_act(), "Check the SD card."));
  assert(visible_thumbnails(lv_scr_act()) == 0U);
  auto* unavailable_capture = visible_label(lv_scr_act(), "Capture");
  assert(unavailable_capture && lv_obj_has_state(lv_obj_get_parent(unavailable_capture), LV_STATE_DISABLED));
  press("Back");
  assert(app->flush_pending_transition());
  assert(!camera.running());
}
