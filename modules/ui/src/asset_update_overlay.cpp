#include "hmi/ui/asset_update_overlay.hpp"

#include <algorithm>
#include <cstdio>

namespace hmi::ui {
namespace {

constexpr lv_coord_t kPanelWidth = 760;
constexpr lv_coord_t kPanelHeight = 330;

bool same_model(const AssetUpdateViewModel& left,
                const AssetUpdateViewModel& right) noexcept {
  return left.state == right.state &&
         left.preparation == right.preparation &&
         left.file_index == right.file_index &&
         left.file_count == right.file_count &&
         left.bytes_received == right.bytes_received &&
         left.total_bytes == right.total_bytes;
}

}  // namespace

AssetUpdateOverlay::~AssetUpdateOverlay() { destroy(); }

bool AssetUpdateOverlay::show() noexcept {
  if (root_ != nullptr) {
    return true;
  }
  lv_obj_t* layer = lv_layer_top();
  if (layer == nullptr) {
    return false;
  }

  root_ = lv_obj_create(layer);
  if (root_ == nullptr) {
    return false;
  }
  lv_obj_add_event_cb(root_, &AssetUpdateOverlay::handle_delete, LV_EVENT_DELETE,
                      this);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_set_style_bg_color(root_, lv_color_hex(0x07111F), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_radius(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* panel = lv_obj_create(root_);
  if (panel == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(panel, kPanelWidth, kPanelHeight);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x10243C), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x2A4968), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, 22, 0);
  lv_obj_set_style_pad_all(panel, 38, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(panel);
  status_label_ = lv_label_create(panel);
  progress_bar_ = lv_bar_create(panel);
  percent_label_ = lv_label_create(panel);
  detail_label_ = lv_label_create(panel);
  if (title == nullptr || status_label_ == nullptr || progress_bar_ == nullptr ||
      percent_label_ == nullptr || detail_label_ == nullptr) {
    destroy();
    return false;
  }

  lv_label_set_text(title, "HMI ASSET UPDATE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x22D3EE), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_label_set_text(status_label_, "Preparing update...");
  lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(status_label_, lv_color_hex(0xF7FAFF), 0);
  lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 0, 42);

  lv_obj_set_size(progress_bar_, LV_PCT(100), 22);
  lv_obj_align(progress_bar_, LV_ALIGN_TOP_LEFT, 0, 108);
  lv_bar_set_range(progress_bar_, 0, 100);
  lv_bar_set_value(progress_bar_, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0x17314F),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0x6D5EF8),
                            LV_PART_INDICATOR);
  lv_obj_set_style_radius(progress_bar_, 11, LV_PART_MAIN);
  lv_obj_set_style_radius(progress_bar_, 11, LV_PART_INDICATOR);

  lv_label_set_text(percent_label_, "0%");
  lv_obj_set_style_text_font(percent_label_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(percent_label_, lv_color_hex(0xF7FAFF), 0);
  lv_obj_align(percent_label_, LV_ALIGN_TOP_RIGHT, 0, 148);

  lv_label_set_text(detail_label_, "Waiting for transfer data");
  lv_obj_set_width(detail_label_, LV_PCT(82));
  lv_label_set_long_mode(detail_label_, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(detail_label_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(detail_label_, lv_color_hex(0x9FB0C8), 0);
  lv_obj_align(detail_label_, LV_ALIGN_TOP_LEFT, 0, 155);

  has_last_ = false;
  return true;
}

void AssetUpdateOverlay::update(const AssetUpdateViewModel& model) noexcept {
  if (!show() || (has_last_ && same_model(last_, model))) {
    return;
  }
  last_ = model;
  has_last_ = true;
  state_ = model.state;

  if (model.state == AssetUpdateViewState::preparing) {
    lv_anim_del(progress_bar_, &AssetUpdateOverlay::set_preparation_progress);
    lv_bar_set_value(progress_bar_, 8, LV_ANIM_OFF);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, progress_bar_);
    lv_anim_set_exec_cb(&animation,
                        &AssetUpdateOverlay::set_preparation_progress);
    lv_anim_set_values(&animation, 8, 76);
    lv_anim_set_time(&animation, 900);
    lv_anim_set_playback_time(&animation, 900);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation);
    progress_percent_ = 0U;
    lv_label_set_text(percent_label_, "...");
    lv_label_set_text(status_label_, "Preparing update...");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0xF7FAFF), 0);
    lv_label_set_text(
        detail_label_,
        model.preparation == AssetUpdatePreparationStep::fonts
            ? "Checking and generating runtime fonts"
            : "Bundling and verifying asset files");
    return;
  }

  lv_anim_del(progress_bar_, &AssetUpdateOverlay::set_preparation_progress);

  const std::uint32_t bounded_received =
      std::min(model.bytes_received, model.total_bytes);
  progress_percent_ = model.total_bytes == 0U
                          ? 0U
                          : static_cast<std::uint8_t>(
                                (static_cast<std::uint64_t>(bounded_received) *
                                 100U) /
                                model.total_bytes);
  if (model.state == AssetUpdateViewState::completed) {
    progress_percent_ = 100U;
  }
  lv_bar_set_value(progress_bar_, progress_percent_, LV_ANIM_OFF);

  char percent[8]{};
  std::snprintf(percent, sizeof(percent), "%u%%",
                static_cast<unsigned>(progress_percent_));
  lv_label_set_text(percent_label_, percent);

  char detail[128]{};
  if (model.state == AssetUpdateViewState::receiving) {
    lv_label_set_text(status_label_, "Updating assets...");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0xF7FAFF), 0);
    std::snprintf(detail, sizeof(detail),
                  "File %u / %u   |   %lu / %lu KB",
                  static_cast<unsigned>(model.file_index),
                  static_cast<unsigned>(model.file_count),
                  static_cast<unsigned long>(bounded_received / 1024U),
                  static_cast<unsigned long>(model.total_bytes / 1024U));
  } else if (model.state == AssetUpdateViewState::completed) {
    lv_label_set_text(status_label_, "Update complete");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0x34D399), 0);
    std::snprintf(detail, sizeof(detail),
                  "Assets installed. Restarting HMI...");
  } else {
    lv_label_set_text(status_label_, "Update failed");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0xFB7185), 0);
    std::snprintf(detail, sizeof(detail),
                  "Transfer stopped. Check the PC message and try again.");
  }
  lv_label_set_text(detail_label_, detail);
}

void AssetUpdateOverlay::destroy() noexcept {
  if (progress_bar_ != nullptr) {
    lv_anim_del(progress_bar_, &AssetUpdateOverlay::set_preparation_progress);
  }
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  status_label_ = nullptr;
  progress_bar_ = nullptr;
  percent_label_ = nullptr;
  detail_label_ = nullptr;
  has_last_ = false;
  progress_percent_ = 0U;
  state_ = AssetUpdateViewState::receiving;
}

void AssetUpdateOverlay::set_preparation_progress(void* bar,
                                                  std::int32_t value) noexcept {
  if (bar != nullptr) {
    lv_bar_set_value(static_cast<lv_obj_t*>(bar), value, LV_ANIM_OFF);
  }
}

void AssetUpdateOverlay::handle_delete(lv_event_t* event) noexcept {
  auto* self = static_cast<AssetUpdateOverlay*>(lv_event_get_user_data(event));
  if (self == nullptr || lv_event_get_target(event) != self->root_) {
    return;
  }
  self->root_ = nullptr;
  self->status_label_ = nullptr;
  self->progress_bar_ = nullptr;
  self->percent_label_ = nullptr;
  self->detail_label_ = nullptr;
  self->has_last_ = false;
}

}  // namespace hmi::ui
