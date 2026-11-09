#include "app.hpp"

#include <array>

namespace hmi::apps::main {
namespace {

constexpr std::array<const char *, 2> kStatusLines = {
    "Profile: main",
    "Status: ready",
};

lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                       const lv_font_t *font, lv_color_t color) {
  lv_obj_t *label = lv_label_create(parent);
  if (label == nullptr) {
    return nullptr;
  }
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
  return label;
}

}  // namespace

bool MainApp::create(const hmi::profiles::AppContext &context) {
  if (screen_ != nullptr || context.host_name == nullptr ||
      context.host_name[0] == '\0') {
    return false;
  }

  screen_ = lv_obj_create(nullptr);
  if (screen_ == nullptr) {
    return false;
  }
  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen_, lv_color_hex(0x111827U), 0);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);

  lv_obj_t *panel = lv_obj_create(screen_);
  if (panel == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(panel, 600, 400);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(panel, &MainApp::handle_input_event, LV_EVENT_PRESSED,
                      this);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x1F2937U), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x475569U), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 20, 0);
  lv_obj_set_style_pad_all(panel, 40, 0);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title = create_label(panel, "HMI Starter", &lv_font_montserrat_32,
                                 lv_color_hex(0xF8FAFCU));
  if (title == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_style_pad_bottom(title, 28, 0);

  lv_obj_t *profile = create_label(panel, kStatusLines[0],
                                   &lv_font_montserrat_24,
                                   lv_color_hex(0xCBD5E1U));
  if (profile == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_style_pad_ver(profile, 8, 0);

  lv_obj_t *host = create_label(panel, "", &lv_font_montserrat_24,
                                lv_color_hex(0xCBD5E1U));
  if (host == nullptr) {
    destroy();
    return false;
  }
  lv_label_set_text_fmt(host, "Host: %s", context.host_name);
  lv_obj_set_style_pad_ver(host, 8, 0);

  for (std::size_t index = 1; index < kStatusLines.size(); ++index) {
    const char *line = kStatusLines[index];
    lv_obj_t *label = create_label(panel, line, &lv_font_montserrat_24,
                                   lv_color_hex(0xCBD5E1U));
    if (label == nullptr) {
      destroy();
      return false;
    }
    lv_obj_set_style_pad_ver(label, 8, 0);
  }

  input_status_ = create_label(panel, "Input: waiting",
                               &lv_font_montserrat_24,
                               lv_color_hex(0x93C5FDU));
  if (input_status_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_style_pad_ver(input_status_, 8, 0);

  lv_scr_load(screen_);
  return true;
}

void MainApp::handle_input_event(lv_event_t *event) {
  if (event == nullptr) {
    return;
  }
  auto *self = static_cast<MainApp *>(lv_event_get_user_data(event));
  if (self != nullptr && self->input_status_ != nullptr) {
    lv_label_set_text(self->input_status_, "Input: detected");
  }
}

void MainApp::destroy() noexcept {
  if (screen_ == nullptr) {
    return;
  }

  if (lv_scr_act() == screen_) {
    lv_obj_t *empty = lv_obj_create(nullptr);
    if (empty != nullptr) {
      lv_scr_load(empty);
    }
  }
  lv_obj_del(screen_);
  screen_ = nullptr;
  input_status_ = nullptr;
}

MainApp::~MainApp() { destroy(); }

}  // namespace hmi::apps::main
