#include "pages/camera_gallery/camera_gallery_page.hpp"

#include <algorithm>
#include <utility>

#include "components/overlay_text.hpp"
#include "generated/fixed_fonts.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {
using hmi::camera::CameraError;
using hmi::camera::CameraGallerySnapshot;
using hmi::camera::CameraSession;
using hmi::camera::GalleryError;
using generated::TextId;
using hmi::ui::ButtonVariant;
using hmi::ui::TextRole;

constexpr std::uint32_t kImageWidth = 620U;
constexpr std::uint32_t kImageHeight = 350U;
constexpr std::uint32_t kThumbnailWidth = 72U;
constexpr std::uint32_t kThumbnailHeight = 40U;
constexpr std::uint32_t kThumbnailPixels = kThumbnailWidth * kThumbnailHeight;

bool valid_frame(const hmi::camera::CameraFrameView& frame) noexcept {
  return frame.pixels && frame.width && frame.height &&
         frame.stride_bytes >= frame.width * 2U && frame.bytes / frame.height >= frame.stride_bytes;
}

void copy_scaled(const hmi::camera::CameraFrameView& frame, lv_color_t* output,
                 std::uint32_t width, std::uint32_t height, std::uint32_t stride) noexcept {
  for (std::uint32_t y = 0; y < height; ++y) {
    const auto* row = frame.pixels + (y * frame.height / height) * frame.stride_bytes;
    for (std::uint32_t x = 0; x < width; ++x) {
      const auto source = 2U * (x * frame.width / width);
      const unsigned rgb = row[source] | (static_cast<unsigned>(row[source + 1U]) << 8U);
      output[y * stride + x] = lv_color_make((rgb >> 8U) & 0xf8U, (rgb >> 3U) & 0xfcU,
                                           (rgb << 3U) & 0xf8U);
    }
  }
}

void scale_thumbnail(const hmi::camera::CameraFrameView& frame, lv_color_t* pixels) noexcept {
  const auto width = std::max<std::uint32_t>(1U, std::min(kThumbnailWidth, kThumbnailHeight * frame.width / frame.height));
  const auto height = std::max<std::uint32_t>(1U, std::min(kThumbnailHeight, kThumbnailWidth * frame.height / frame.width));
  std::fill_n(pixels, kThumbnailPixels, lv_color_hex(0x151a22));
  auto* inner = pixels + (kThumbnailHeight - height) / 2U * kThumbnailWidth + (kThumbnailWidth - width) / 2U;
  copy_scaled(frame, inner, width, height, kThumbnailWidth);
}

void show(lv_obj_t* widget, bool visible) noexcept {
  if (visible) {
    lv_obj_clear_flag(widget, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(widget, LV_OBJ_FLAG_HIDDEN);
  }
}

void enable(lv_obj_t* widget, bool enabled) noexcept {
  if (enabled) {
    lv_obj_clear_state(widget, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(widget, LV_STATE_DISABLED);
  }
}

lv_obj_t* group(lv_obj_t* parent, hmi::ui::UiTheme& theme, lv_flex_flow_t flow,
                bool surface = false) noexcept {
  auto* object = lv_obj_create(parent);
  if (object) {
    if (surface) {
      theme.apply_surface(object);
    } else {
      theme.apply_transparent(object);
      lv_obj_set_style_pad_all(object, 0, 0);
    }
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(object, flow);
    lv_obj_set_style_pad_gap(object, 8, 0);
  }
  return object;
}

TextId status_text(const CameraGallerySnapshot& state) noexcept {
  if (state.session == CameraSession::starting) return TextId::camera_gallery_starting;
  if (state.session == CameraSession::capturing) return TextId::camera_gallery_saving;
  if (state.camera_error != CameraError::none) return TextId::camera_gallery_unavailable;
  switch (state.storage_error) {
    case GalleryError::not_ready: return TextId::camera_gallery_no_sd;
    case GalleryError::pc_owner: return TextId::camera_gallery_pc_owner;
    case GalleryError::lease_busy: return TextId::camera_gallery_busy;
    case GalleryError::full: return TextId::camera_gallery_full;
    case GalleryError::write_failed: return TextId::camera_gallery_write_error;
    case GalleryError::invalid_file: return TextId::camera_gallery_read_error;
    case GalleryError::delete_failed: return TextId::camera_gallery_delete_error;
    case GalleryError::none: break;
  }
  if (state.storage_busy) return TextId::camera_gallery_loading;
  if (state.last_saved_id != 0U && state.session == CameraSession::previewing) {
    return TextId::camera_gallery_saved;
  }
  return TextId::count;
}
}  // namespace

CameraGalleryPage::~CameraGalleryPage() {
  destroy();
  if (controller_) controller_->stop();
  if (thumbnail_pixels_) lv_mem_free(thumbnail_pixels_);
}

void CameraGalleryPage::bind_runtime(const PageRuntime& runtime) noexcept {
  controller_ = runtime.camera_controller;
}

bool CameraGalleryPage::create(lv_obj_t* parent, const PageContext& context) {
  if (root_ || !parent || !context.theme || !context.theme->initialized() ||
      !context.request_dashboard || !context.request_theme_toggle || !context.set_locale) {
    return false;
  }
  context_ = context;
  root_ = lv_obj_create(parent);
  if (!root_) return false;
  configure_page_shell_root(root_, *context_.theme);
  header_ = create_page_header(root_, {TextId::page_camera_gallery_title, TextId::count,
      context.locale, 0, on_event, on_event, on_event, on_event, this}, *context_.theme);
  if (!header_ || !create_body()) {
    destroy();
    return false;
  }
  timer_ = lv_timer_create(on_timer, 60U, this);
  if (!timer_) {
    destroy();
    return false;
  }
  render();
  return true;
}

lv_obj_t* CameraGalleryPage::label(lv_obj_t* parent, TextId text, TextRole role) noexcept {
  auto* result = hmi::ui::create_label(parent, generated::text(context_.locale, text).data(),
                                       *context_.theme, role);
  if (result) lv_obj_set_style_text_font(result, generated::fixed_font(generated::typography(text)), 0);
  return result;
}

lv_obj_t* CameraGalleryPage::button(lv_obj_t* parent, TextId text, ButtonVariant variant) noexcept {
  auto* result = hmi::ui::create_button(parent, generated::text(context_.locale, text).data(),
                                        *context_.theme, variant, on_event, this);
  if (result) {
    lv_obj_set_height(result, 44);
    lv_obj_set_style_text_font(lv_obj_get_child(result, 0), generated::fixed_font(generated::typography(text)), 0);
  }
  return result;
}

bool CameraGalleryPage::create_body() noexcept {
  auto& theme = *context_.theme;
  auto* body = group(root_, theme, LV_FLEX_FLOW_ROW);
  if (!body) return false;
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  auto* stage = group(body, theme, LV_FLEX_FLOW_COLUMN, true);
  auto* gallery = group(body, theme, LV_FLEX_FLOW_COLUMN, true);
  if (!stage || !gallery) return false;
  lv_obj_set_size(stage, 648, LV_PCT(100));
  lv_obj_set_height(gallery, LV_PCT(100));
  lv_obj_set_flex_grow(gallery, 1);
  lv_obj_set_style_pad_all(stage, 10, 0);
  lv_obj_set_style_pad_all(gallery, 12, 0);

  auto* viewport = lv_obj_create(stage);
  if (!viewport) return false;
  lv_obj_remove_style_all(viewport);
  lv_obj_set_size(viewport, LV_PCT(100), kImageHeight);
  lv_obj_set_style_bg_color(viewport, lv_color_hex(0x151a22), 0);
  lv_obj_set_style_bg_opa(viewport, LV_OPA_COVER, 0);
  lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
  image_ = lv_img_create(viewport);
  status_ = label(stage, TextId::camera_gallery_unavailable, TextRole::kMuted);
  auto* actions = group(stage, theme, LV_FLEX_FLOW_ROW);
  if (!image_ || !status_ || !actions) return false;
  lv_obj_center(image_);
  lv_label_set_text(status_, "");
  lv_obj_set_size(status_, LV_PCT(100), 24);
  lv_label_set_long_mode(status_, LV_LABEL_LONG_DOT);
  lv_obj_set_size(actions, LV_PCT(100), 44);
  capture_ = button(actions, TextId::camera_gallery_capture, ButtonVariant::kPrimary);
  preview_ = button(actions, TextId::camera_gallery_preview, ButtonVariant::kSecondary);
  retry_ = button(actions, TextId::camera_gallery_retry, ButtonVariant::kSecondary);
  delete_ = button(actions, TextId::camera_gallery_delete, ButtonVariant::kSecondary);

  auto* heading = label(gallery, TextId::camera_gallery_gallery, TextRole::kTitle);
  auto* list = group(gallery, theme, LV_FLEX_FLOW_COLUMN);
  auto* paging = group(gallery, theme, LV_FLEX_FLOW_ROW);
  if (!heading || !list || !paging || !capture_ || !preview_ || !retry_ || !delete_) return false;
  lv_obj_set_width(capture_, 136);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  empty_ = label(list, TextId::camera_gallery_empty, TextRole::kMuted);
  if (!empty_) return false;
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    auto*& row = rows_[index];
    row = button(list, TextId::camera_gallery_gallery, ButtonVariant::kSecondary);
    if (!row) return false;
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 54);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_bg_color(row, lv_color_mix(theme.accent_color(),
        lv_obj_get_style_bg_color(row, LV_PART_MAIN), 48), LV_STATE_CHECKED);
    lv_obj_set_style_border_color(row, theme.accent_color(), LV_STATE_CHECKED);
    auto* name = lv_obj_get_child(row, 0);
    lv_obj_set_style_text_font(name, LV_FONT_DEFAULT, 0);
    lv_obj_set_flex_grow(name, 1);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    row_images_[index] = lv_img_create(row);
    row_icons_[index] = lv_label_create(row);
    if (!row_images_[index] || !row_icons_[index]) return false;
    lv_obj_move_to_index(row_images_[index], 0);
    lv_obj_move_to_index(row_icons_[index], 0);
    lv_label_set_text(row_icons_[index], LV_SYMBOL_IMAGE);
    lv_obj_set_width(row_icons_[index], kThumbnailWidth);
    lv_obj_set_style_text_align(row_icons_[index], LV_TEXT_ALIGN_CENTER, 0);
    show(row_images_[index], false);
    // 목록 갱신은 자식 순서에 의존하지 않도록 파일명 label을 마지막에 둔다.
    lv_obj_move_to_index(name, -1);
  }
  lv_obj_set_size(paging, LV_PCT(100), 44);
  previous_ = button(paging, TextId::camera_gallery_previous, ButtonVariant::kGhost);
  next_ = button(paging, TextId::camera_gallery_next, ButtonVariant::kGhost);
  return previous_ && next_;
}

void CameraGalleryPage::open_confirmation(hmi::camera::PhotoId id) noexcept {
  confirmation_ = hmi::ui::show_confirmation(root_, *context_.theme, {
      overlay_text(context_.locale, TextId::camera_gallery_confirm), {},
      overlay_text(context_.locale, TextId::camera_gallery_cancel),
      overlay_text(context_.locale, TextId::camera_gallery_delete), on_event, on_event, this});
  if (confirmation_) confirm_id_ = id;
}

void CameraGalleryPage::hide_confirmation() noexcept {
  confirm_id_ = 0U;
  confirmation_.close();
}

PageSnapshot CameraGalleryPage::snapshot() const noexcept {
  PageSnapshot state;
  if (confirm_id_) state.overlay = OverlayState::kConfirm;
  return state;
}

void CameraGalleryPage::on_event(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) return;
  auto& self = *static_cast<CameraGalleryPage*>(lv_event_get_user_data(event));
  auto* target = lv_event_get_target(event);
  if (!lv_obj_has_state(target, LV_STATE_DISABLED)) self.handle_action(target);
}

void CameraGalleryPage::handle_action(lv_obj_t* target) noexcept {
  if (target == confirmation_.cancel) {
    hide_confirmation();
    return;
  }
  if (target == confirmation_.confirm) {
    const auto id = confirm_id_;
    hide_confirmation();
    if (controller_ && controller_->delete_selected(id)) {
      for (auto& thumbnail : thumbnails_) {
        if (thumbnail.id == id) thumbnail.id = 0U;
      }
      render_thumbnails();
    }
    return;
  }
  if (confirm_id_) return;
  if (target == header_.locale_ko_button || target == header_.locale_en_button) {
    context_.set_locale(context_.user_data, target == header_.locale_ko_button
        ? hmi::i18n::Locale::ko_kr : hmi::i18n::Locale::en_us);
    return;
  }
  if (target == header_.theme_button) {
    context_.request_theme_toggle(context_.user_data);
    return;
  }
  if (target == header_.back_button) {
    if (controller_ && controller_->snapshot().session == CameraSession::viewing) {
      controller_->back_to_preview();
    } else {
      context_.request_dashboard(context_.user_data);
    }
    return;
  }
  if (!controller_) return;
  const auto& state = controller_->snapshot();
  if (target == capture_) {
    controller_->capture();
  } else if (target == preview_) {
    controller_->back_to_preview();
  } else if (target == retry_) {
    controller_->start();
  } else if (target == delete_ && state.selected_id && !state.storage_busy) {
    open_confirmation(state.selected_id);
  } else if (target == previous_ && state.page_index > 0U) {
    controller_->refresh(state.page_index - 1U);
  } else if (target == next_ && (state.page_index + 1U) * rows_.size() < state.item_count) {
    controller_->refresh(state.page_index + 1U);
  } else {
    const auto& page = controller_->page();
    const auto found = std::find(rows_.begin(), rows_.end(), target);
    const auto index = static_cast<std::size_t>(found - rows_.begin());
    if (index < page.count) controller_->open(page.items[index].id);
  }
}

void CameraGalleryPage::on_timer(lv_timer_t* timer) {
  static_cast<CameraGalleryPage*>(timer->user_data)->render();
}

void CameraGalleryPage::on_capture_frame(void* context, const hmi::camera::CameraFrameView& frame) noexcept {
  auto& page = *static_cast<CameraGalleryPage*>(context);
  page.capture_thumbnail_pending_ = false;
  if (!valid_frame(frame)) return;
  // 촬영 lease가 반환되기 전에 축소본만 보관하고 파일 저장 성공 시 목록 캐시에 넣는다.
  auto* pixels = page.thumbnail_buffer(page.thumbnails_.size());
  if (!pixels) return;
  scale_thumbnail(frame, pixels);
  page.capture_thumbnail_pending_ = true;
}

void CameraGalleryPage::render_list() noexcept {
  const auto* page = controller_ ? &controller_->page() : nullptr;
  const auto count = page ? page->count : 0U;
  show(empty_, count == 0U);
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    show(rows_[index], index < count);
    if (index < count) {
      const auto name = hmi::camera::photo_filename(page->items[index].id);
      lv_label_set_text(lv_obj_get_child(rows_[index], -1), name.data());
    }
  }
  render_thumbnails();
}

void CameraGalleryPage::render() noexcept {
  CameraGallerySnapshot unavailable{};
  unavailable.camera_error = CameraError::unsupported;
  if (controller_) {
    if (context_.storage_access) {
      const auto access = context_.storage_access->snapshot();
      controller_->update_storage(access);
      if (access.generation != media_generation_ || access.rescan_revision != media_rescan_ ||
          (access.lock_required && next_thumbnail_ != 0U)) {
        for (auto& thumbnail : thumbnails_) thumbnail.id = 0U;
        next_thumbnail_ = 0U;
        capture_thumbnail_pending_ = false;
        media_generation_ = access.generation;
        media_rescan_ = access.rescan_revision;
        render_thumbnails();
      }
    }
    if (controller_->snapshot().session == CameraSession::idle &&
        controller_->snapshot().storage_error != GalleryError::pc_owner) controller_->start();
    controller_->poll(on_capture_frame, this);
  }
  const auto& state = controller_ ? controller_->snapshot() : unavailable;
  if (state.session != CameraSession::capturing && std::exchange(capture_thumbnail_pending_, false) &&
      state.last_saved_id != 0U) {
    if (auto* pixels = cache_thumbnail(state.last_saved_id)) {
      std::copy_n(thumbnail_buffer(thumbnails_.size()), kThumbnailPixels, pixels);
      render_thumbnails();
    }
  }
  const bool viewing = state.session == CameraSession::viewing;
  const bool live = state.session == CameraSession::previewing;
  const bool actionable = state.storage_ready && !state.storage_busy &&
                          state.session != CameraSession::capturing;
  show(capture_, !viewing && state.session != CameraSession::failed);
  enable(capture_, live && actionable);
  show(preview_, viewing);
  enable(preview_, !state.storage_busy);
  show(retry_, controller_ && state.session == CameraSession::failed);
  show(delete_, viewing);
  enable(delete_, state.selected_id && actionable);
  const bool multiple_pages = state.item_count > rows_.size();
  show(previous_, multiple_pages);
  show(next_, multiple_pages);
  enable(previous_, actionable && state.page_index > 0U);
  enable(next_, actionable && (state.page_index + 1U) * rows_.size() < state.item_count);
  if (confirm_id_ && (!actionable || state.selected_id != confirm_id_)) hide_confirmation();
  const auto status = status_text(state);
  if (status != shown_status_) {
    shown_status_ = status;
    if (status == TextId::count) {
      lv_label_set_text(status_, "");
    } else {
      lv_label_set_text(status_, generated::text(context_.locale, status).data());
      lv_obj_set_style_text_font(status_, generated::fixed_font(generated::typography(status)), 0);
    }
  }
  if (state.gallery_revision != drawn_revision_) {
    drawn_revision_ = state.gallery_revision;
    render_list();
  }
  const auto* page = controller_ ? &controller_->page() : nullptr;
  for (std::size_t index = 0; page && index < page->count; ++index) {
    enable(rows_[index], actionable);
    if (state.selected_id == page->items[index].id) {
      lv_obj_add_state(rows_[index], LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(rows_[index], LV_STATE_CHECKED);
    }
  }
  const auto* frame = controller_ ? controller_->image() : nullptr;
  if (frame && (live || (viewing && drawn_photo_ != state.selected_id))) {
    if (draw_frame(*frame)) {
      drawn_photo_ = viewing ? state.selected_id : 0U;
      if (viewing) remember_thumbnail(state.selected_id, *frame);
    }
  } else if (!frame && !live && state.session != CameraSession::capturing) {
    show(image_, false);
    drawn_photo_ = 0U;
  }
  if (live) controller_->release_preview();
}

bool CameraGalleryPage::draw_frame(const hmi::camera::CameraFrameView& frame) noexcept {
  if (!valid_frame(frame)) return false;
  if (!pixels_) {
    // LVGL의 host allocator를 사용한다. HMI는 이 크기의 표시 버퍼를 PSRAM에 우선 할당한다.
    pixels_ = static_cast<lv_color_t*>(lv_mem_alloc(kImageWidth * kImageHeight * sizeof(lv_color_t)));
    if (!pixels_) return false;
  }
  const auto width = std::max<std::uint32_t>(1U, std::min(kImageWidth, kImageHeight * frame.width / frame.height));
  const auto height = std::max<std::uint32_t>(1U, std::min(kImageHeight, kImageWidth * frame.height / frame.width));
  copy_scaled(frame, pixels_, width, height, width);
  if (!descriptor_.data || descriptor_.header.w != width || descriptor_.header.h != height) {
    lv_img_cache_invalidate_src(&descriptor_);
    descriptor_.header.cf = LV_IMG_CF_TRUE_COLOR;
    descriptor_.header.w = width;
    descriptor_.header.h = height;
    descriptor_.data_size = width * height * sizeof(lv_color_t);
    descriptor_.data = reinterpret_cast<const std::uint8_t*>(pixels_);
    lv_img_set_src(image_, &descriptor_);
    lv_obj_center(image_);
  }
  show(image_, true);
  lv_obj_invalidate(image_);
  return true;
}

void CameraGalleryPage::remember_thumbnail(hmi::camera::PhotoId id,
                                           const hmi::camera::CameraFrameView& frame) noexcept {
  const auto known = std::find_if(thumbnails_.begin(), thumbnails_.end(),
      [id](const auto& thumbnail) { return thumbnail.id == id; });
  if (known != thumbnails_.end()) return;
  if (auto* pixels = cache_thumbnail(id)) {
    scale_thumbnail(frame, pixels);
    render_thumbnails();
  }
}

lv_color_t* CameraGalleryPage::thumbnail_buffer(std::size_t slot) noexcept {
  if (!thumbnail_pixels_) {
    // 표시 캐시 네 슬롯과 저장 결과를 기다리는 촬영용 슬롯 하나를 함께 할당한다.
    thumbnail_pixels_ = static_cast<lv_color_t*>(
        lv_mem_alloc((thumbnails_.size() + 1U) * kThumbnailPixels * sizeof(lv_color_t)));
  }
  return thumbnail_pixels_ ? thumbnail_pixels_ + slot * kThumbnailPixels : nullptr;
}

lv_color_t* CameraGalleryPage::cache_thumbnail(hmi::camera::PhotoId id) noexcept {
  const auto slot = next_thumbnail_ % thumbnails_.size();
  auto* pixels = thumbnail_buffer(slot);
  if (!pixels) return nullptr;
  ++next_thumbnail_;
  auto& thumb = thumbnails_[slot];
  lv_img_cache_invalidate_src(&thumb.image);
  thumb.id = id;
  thumb.image.header.cf = LV_IMG_CF_TRUE_COLOR;
  thumb.image.header.w = kThumbnailWidth;
  thumb.image.header.h = kThumbnailHeight;
  thumb.image.data_size = kThumbnailPixels * sizeof(lv_color_t);
  thumb.image.data = reinterpret_cast<const std::uint8_t*>(pixels);
  return pixels;
}

void CameraGalleryPage::render_thumbnails() noexcept {
  const auto* page = controller_ ? &controller_->page() : nullptr;
  for (std::size_t index = 0; page && index < page->count; ++index) {
    const auto id = page->items[index].id;
    const auto found = std::find_if(thumbnails_.begin(), thumbnails_.end(),
        [id](const auto& thumbnail) { return thumbnail.id == id; });
    const bool available = found != thumbnails_.end();
    if (available) {
      lv_img_set_src(row_images_[index], &found->image);
      lv_obj_invalidate(row_images_[index]);
    }
    show(row_images_[index], available);
    show(row_icons_[index], !available);
  }
}

void CameraGalleryPage::destroy() noexcept {
  if (timer_) lv_timer_del(std::exchange(timer_, nullptr));
  hide_confirmation();
  if (root_) lv_obj_del(std::exchange(root_, nullptr));
  for (auto& thumb : thumbnails_) {
    if (thumb.image.data) lv_img_cache_invalidate_src(&thumb.image);
  }
  if (pixels_) {
    lv_img_cache_invalidate_src(&descriptor_);
    lv_mem_free(std::exchange(pixels_, nullptr));
  }
  descriptor_ = {};
  drawn_photo_ = 0U;
  drawn_revision_ = UINT32_MAX;
  shown_status_ = TextId::count;
}

}  // namespace hmi::samples::suite
