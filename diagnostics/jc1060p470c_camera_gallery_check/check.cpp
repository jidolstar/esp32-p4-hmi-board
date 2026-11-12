#include "diagnostics/jc1060p470c_camera_gallery_check/check.hpp"

#include <cstdio>
#include <cstring>
#include <inttypes.h>

#include <esp_heap_caps.h>
#include <lvgl.h>

#include "boards/jc1060p470c/board.hpp"
#include "camera/camera_gallery_worker.hpp"
#include "hmi/camera/camera_gallery_controller.hpp"
#include "log.hpp"

namespace hmi::diagnostics::camera_gallery {
namespace {
using namespace hmi::camera;

constexpr std::uint16_t kViewWidth = 644U;
constexpr std::uint16_t kViewHeight = 364U;
constexpr std::uint32_t kViewBytes = kViewWidth * kViewHeight * 2U;
enum class Action : std::uint8_t { capture, view, preview, expose, reclaim };

/** C2 실기기 검증 전용 화면이다. 진단 화면이 픽셀을 복사한 뒤 카메라 lease를 반환한다. */
class Check {
 public:
  Check(boards::jc1060p470c::Board& board, storage::StorageAccessController& storage,
        const char* root) noexcept
      : worker_(board.camera(), board.storage(), root), controller_(worker_, worker_), storage_(storage) {}

  bool create() noexcept;
  void tick(std::uint32_t now_ms) noexcept;

 private:
  struct Button { Check* screen; Action action; };
  void add_button(lv_obj_t* parent, Button& button, const char* caption) noexcept;
  static void clicked(lv_event_t* event) noexcept;
  void copy_frame(const CameraFrameView& view) noexcept;
  void update_status() noexcept;

  platform::hmi_host::camera::CameraGalleryWorker worker_;
  CameraGalleryController controller_;
  storage::StorageAccessController& storage_;
  lv_obj_t* image_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_img_dsc_t descriptor_{};
  std::uint8_t* pixels_ = nullptr;
  std::uint32_t now_ms_ = 0U;
  std::uint32_t last_draw_ms_ = 0U;
  std::uint32_t capture_revision_ = 0U;
  PhotoId displayed_id_ = 0U;
  bool logged_frame_ = false;
  Button buttons_[5]{{this, Action::capture}, {this, Action::view}, {this, Action::preview},
                      {this, Action::expose}, {this, Action::reclaim}};
};

Check* active_check = nullptr;

void Check::add_button(lv_obj_t* parent, Button& button, const char* caption) noexcept {
  auto* object = lv_btn_create(parent);
  lv_obj_set_size(object, 126, 44);
  lv_obj_add_event_cb(object, clicked, LV_EVENT_CLICKED, &button);
  auto* label = lv_label_create(object);
  lv_label_set_text(label, caption);
  lv_obj_center(label);
}

bool Check::create() noexcept {
  pixels_ = static_cast<std::uint8_t*>(heap_caps_calloc(1U, kViewBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!pixels_ || !worker_.begin()) {
    heap_caps_free(pixels_);
    pixels_ = nullptr;
    return false;
  }
  descriptor_.header.cf = LV_IMG_CF_TRUE_COLOR;
  descriptor_.header.w = kViewWidth;
  descriptor_.header.h = kViewHeight;
  descriptor_.data_size = kViewBytes;
  descriptor_.data = pixels_;
  auto* screen = lv_scr_act();
  status_ = lv_label_create(screen);
  lv_obj_align(status_, LV_ALIGN_TOP_MID, 0, 22);
  image_ = lv_img_create(screen);
  lv_img_set_src(image_, &descriptor_);
  lv_obj_align(image_, LV_ALIGN_CENTER, 0, -12);

  auto* controls = lv_obj_create(screen);
  lv_obj_set_size(controls, 720, 72);
  lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
  add_button(controls, buttons_[0], "Capture");
  add_button(controls, buttons_[1], "View latest");
  add_button(controls, buttons_[2], "Preview");
  add_button(controls, buttons_[3], "USB MSC");
  add_button(controls, buttons_[4], "Return to HMI");
  storage_.tick(0U);
  controller_.update_storage(storage_.snapshot());
  controller_.start();
  update_status();
  return true;
}

void Check::clicked(lv_event_t* event) noexcept {
  const auto& button = *static_cast<Button*>(lv_event_get_user_data(event));
  auto& self = *button.screen;
  switch (button.action) {
    case Action::capture: self.controller_.capture(); break;
    case Action::view:
      if (self.controller_.page().count != 0U) {
        self.controller_.open(self.controller_.page().items[0].id);
      }
      break;
    case Action::preview:
      if (self.controller_.snapshot().session == CameraSession::viewing) {
        self.controller_.back_to_preview();
      } else {
        self.controller_.start();
      }
      break;
    case Action::expose: self.storage_.request_pc_access(self.now_ms_); break;
    case Action::reclaim:
      if (self.storage_.snapshot().state == storage::StorageAccessState::unavailable) {
        self.storage_.retry_media(self.now_ms_);
      } else {
        self.storage_.request_hmi_access(self.now_ms_, storage::StorageReclaimReason::explicit_request);
      }
      break;
  }
}

void Check::copy_frame(const CameraFrameView& view) noexcept {
  if (!view.pixels || !view.width || !view.height || view.stride_bytes < view.width * 2U ||
      view.bytes < view.stride_bytes * view.height) {
    return;
  }
  for (std::uint32_t y = 0U; y < kViewHeight; ++y) {
    const auto* row = view.pixels + (y * view.height / kViewHeight) * view.stride_bytes;
    for (std::uint32_t x = 0U; x < kViewWidth; ++x) {
      const auto src = (x * view.width / kViewWidth) * 2U;
      const auto dst = (y * kViewWidth + x) * 2U;
      pixels_[dst] = row[src];
      pixels_[dst + 1U] = row[src + 1U];
    }
  }
  lv_obj_invalidate(image_);
}

void Check::update_status() noexcept {
  const auto& state = controller_.snapshot();
  const auto sd = storage_.snapshot();
  const char* session = "Idle";
  switch (state.session) {
    case CameraSession::idle: break;
    case CameraSession::starting: session = "Starting"; break;
    case CameraSession::previewing: session = "Preview"; break;
    case CameraSession::capturing: session = "Capturing"; break;
    case CameraSession::viewing: session = "Viewer"; break;
    case CameraSession::failed: session = "Camera error"; break;
  }
  char text[192]{};
  std::snprintf(text, sizeof(text), "%s | SD %s | camera %u | storage %u | saved %" PRIu64 " | photos %u",
                session, sd.owner == storage::StorageOwner::usb_msc ? "PC" : (state.storage_ready ? "HMI" : "unavailable"),
                static_cast<unsigned>(state.camera_error), static_cast<unsigned>(state.storage_error),
                state.last_saved_id, static_cast<unsigned>(state.item_count));
  if (std::strcmp(text, lv_label_get_text(status_)) != 0) {
    lv_label_set_text(status_, text);
    HMI_LOG_INFO("[C2] %s\r\n", text);
  }
}

void Check::tick(std::uint32_t now_ms) noexcept {
  now_ms_ = now_ms;
  // S05가 raw owner를 요청하기 전에 완료 대기와 프레임 해제를 수행한다.
  controller_.update_storage(storage_.snapshot());
  storage_.tick(now_ms);
  controller_.update_storage(storage_.snapshot());
  controller_.poll();
  const auto& state = controller_.snapshot();
  if (state.session == CameraSession::previewing) {
    const auto* frame = controller_.image();
    if (frame && (!logged_frame_ || now_ms - last_draw_ms_ >= 100U)) {
      copy_frame(*frame);
      last_draw_ms_ = now_ms;
      if (!logged_frame_) {
        HMI_LOG_INFO("[C2] RGB565 %ux%u stride=%u bytes=%u\r\n",
                     static_cast<unsigned>(frame->width), static_cast<unsigned>(frame->height),
                     static_cast<unsigned>(frame->stride_bytes), static_cast<unsigned>(frame->bytes));
        logged_frame_ = true;
      }
    }
    controller_.release_preview();
    displayed_id_ = 0U;
  } else if (state.session == CameraSession::viewing && state.selected_id != displayed_id_ && controller_.image()) {
    copy_frame(*controller_.image());
    displayed_id_ = state.selected_id;
    HMI_LOG_INFO("[C2] JPEG viewed id=%" PRIu64 "\r\n", displayed_id_);
  }
  if (state.capture_revision != capture_revision_) {
    capture_revision_ = state.capture_revision;
    if (state.last_saved_id != 0U) {
      HMI_LOG_INFO("[C2] JPEG saved IMG_%" PRIu64 ".jpg\r\n", state.last_saved_id);
    }
  }
  update_status();
}
}  // namespace

bool begin(boards::jc1060p470c::Board& board, storage::StorageAccessController& storage,
           const char* root) noexcept {
  static Check check(board, storage, root);
  if (!check.create()) {
    return false;
  }
  active_check = &check;
  return true;
}

void tick(std::uint32_t now_ms) noexcept {
  if (active_check) {
    active_check->tick(now_ms);
  }
}

}  // namespace hmi::diagnostics::camera_gallery
