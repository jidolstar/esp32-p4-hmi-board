#pragma once

#include <array>

#include "components/page_header.hpp"
#include "hmi/camera/camera_gallery_controller.hpp"
#include "hmi/ui/components.hpp"
#include "page_contract.hpp"

namespace hmi::samples::suite {

/** 화면과 표시 버퍼를 소유한다. locale/theme 재생성 동안 host controller는 계속 살아 있다. */
class CameraGalleryPage final : public SamplePage {
 public:
  ~CameraGalleryPage() override;
  void bind_runtime(const PageRuntime& runtime) noexcept override;
  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override;

 private:
  static void on_event(lv_event_t* event);
  static void on_timer(lv_timer_t* timer);
  static void on_capture_frame(void* context, const hmi::camera::CameraFrameView& frame) noexcept;
  bool create_body() noexcept;
  void open_confirmation(hmi::camera::PhotoId id) noexcept;
  void handle_action(lv_obj_t* target) noexcept;
  void render() noexcept;
  void render_list() noexcept;
  bool draw_frame(const hmi::camera::CameraFrameView& frame) noexcept;
  void remember_thumbnail(hmi::camera::PhotoId id, const hmi::camera::CameraFrameView& frame) noexcept;
  lv_color_t* thumbnail_buffer(std::size_t slot) noexcept;
  lv_color_t* cache_thumbnail(hmi::camera::PhotoId id) noexcept;
  void render_thumbnails() noexcept;
  void hide_confirmation() noexcept;
  lv_obj_t* label(lv_obj_t* parent, generated::TextId text, hmi::ui::TextRole role) noexcept;
  lv_obj_t* button(lv_obj_t* parent, generated::TextId text, hmi::ui::ButtonVariant variant) noexcept;

  PageContext context_{};
  hmi::camera::CameraGalleryController* controller_ = nullptr;
  PageHeaderWidgets header_{};
  lv_obj_t* root_ = nullptr;
  lv_obj_t* image_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* empty_ = nullptr;
  lv_obj_t* capture_ = nullptr;
  lv_obj_t* preview_ = nullptr;
  lv_obj_t* retry_ = nullptr;
  lv_obj_t* delete_ = nullptr;
  lv_obj_t* previous_ = nullptr;
  lv_obj_t* next_ = nullptr;
  hmi::ui::ConfirmationWidgets confirmation_{};
  std::array<lv_obj_t*, hmi::camera::kGalleryPageCapacity> rows_{};
  std::array<lv_obj_t*, hmi::camera::kGalleryPageCapacity> row_images_{};
  std::array<lv_obj_t*, hmi::camera::kGalleryPageCapacity> row_icons_{};
  struct Thumbnail {
    hmi::camera::PhotoId id = 0U;
    lv_img_dsc_t image{};
  };
  std::array<Thumbnail, 4U> thumbnails_{};
  std::size_t next_thumbnail_ = 0U;
  bool capture_thumbnail_pending_ = false;
  std::uint32_t media_generation_ = 0U;
  std::uint32_t media_rescan_ = 0U;
  lv_timer_t* timer_ = nullptr;
  lv_color_t* pixels_ = nullptr;
  lv_color_t* thumbnail_pixels_ = nullptr;
  lv_img_dsc_t descriptor_{};
  hmi::camera::PhotoId confirm_id_ = 0U;
  hmi::camera::PhotoId drawn_photo_ = 0U;
  std::uint32_t drawn_revision_ = UINT32_MAX;
  generated::TextId shown_status_ = generated::TextId::count;
};

}  // namespace hmi::samples::suite
