#include "hmi/ui/components.hpp"

namespace hmi::ui {

lv_obj_t *create_label(lv_obj_t *parent, const char *text, UiTheme &theme,
                       TextRole role) noexcept {
  if (parent == nullptr || text == nullptr) {
    return nullptr;
  }
  lv_obj_t *label = lv_label_create(parent);
  if (label == nullptr) {
    return nullptr;
  }
  lv_label_set_text(label, text);
  theme.apply_label(label, role);
  return label;
}

lv_obj_t *create_button(lv_obj_t *parent, const char *text, UiTheme &theme,
                        ButtonVariant variant, lv_event_cb_t callback,
                        void *user_data) noexcept {
  if (parent == nullptr || text == nullptr || callback == nullptr) {
    return nullptr;
  }
  lv_obj_t *button = lv_btn_create(parent);
  if (button == nullptr) {
    return nullptr;
  }
  theme.apply_button(button, variant);
  lv_obj_add_event_cb(button, callback, LV_EVENT_ALL, user_data);
  const TextRole role = variant == ButtonVariant::kPrimary
                            ? TextRole::kInverse
                            : TextRole::kBody;
  lv_obj_t *label = create_label(button, text, theme, role);
  if (label == nullptr) {
    lv_obj_del(button);
    return nullptr;
  }
  lv_obj_center(label);
  return button;
}

void configure_multiline(lv_obj_t *label, lv_coord_t width, lv_coord_t height,
                         lv_coord_t line_space, lv_text_align_t align) noexcept {
  if (label == nullptr) {
    return;
  }
  lv_obj_set_size(label, width, height);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_line_space(label, line_space, 0);
  lv_obj_set_style_text_align(label, align, 0);
}

}  // namespace hmi::ui
