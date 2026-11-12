#include "hmi/ui/components.hpp"

#include <algorithm>
#include <utility>

namespace hmi::ui {
namespace {

void configure_overlay_text(lv_obj_t* label, const lv_font_t* font) noexcept {
  if (font) lv_obj_set_style_text_font(label, font, 0);
  configure_multiline(label, LV_PCT(100), LV_SIZE_CONTENT, 6, LV_TEXT_ALIGN_CENTER);
}

lv_obj_t* overlay_label(lv_obj_t* parent, OverlayText text, UiTheme& theme,
                        TextRole role) noexcept {
  auto* label = create_label(parent, text.text, theme, role);
  if (label) configure_overlay_text(label, text.font);
  return label;
}

lv_obj_t* confirmation_button(lv_obj_t* parent, OverlayText text, UiTheme& theme,
                              ButtonVariant variant, lv_event_cb_t callback,
                              void* context) noexcept {
  auto* button = create_button(parent, text.text, theme, variant, callback, context);
  if (!button) return nullptr;
  lv_obj_set_width(button, 0);
  lv_obj_set_height(button, 52);
  lv_obj_set_flex_grow(button, 1);
  configure_overlay_text(lv_obj_get_child(button, 0), text.font);
  return button;
}

lv_coord_t panel_width(lv_obj_t* screen) noexcept {
  return std::min<lv_coord_t>(520, lv_disp_get_hor_res(lv_obj_get_disp(screen)) - 32);
}

}  // namespace

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

void ConfirmationWidgets::close() noexcept {
  auto* layer = scrim;
  *this = {};
  if (layer) lv_obj_del(layer);
}

ConfirmationWidgets show_confirmation(lv_obj_t* anchor, UiTheme& theme,
                                       const ConfirmationSpec& spec) noexcept {
  if (!anchor || !spec.title.text || !spec.confirm.text || !spec.on_confirm ||
      (spec.cancel.text && !spec.on_cancel)) return {};
  auto* screen = lv_obj_get_screen(anchor);
  if (panel_width(screen) <= 0) return {};
  ConfirmationWidgets widgets;
  widgets.scrim = lv_obj_create(screen);
  if (!widgets.scrim) return {};
  // 화면에 직접 붙여 page의 padding과 flex가 입력 차단 영역에 영향을 주지 않게 한다.
  lv_obj_remove_style_all(widgets.scrim);
  theme.apply_scrim(widgets.scrim);
  lv_obj_add_flag(widgets.scrim, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(widgets.scrim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_set_size(widgets.scrim, lv_disp_get_hor_res(lv_obj_get_disp(screen)),
                  lv_disp_get_ver_res(lv_obj_get_disp(screen)));
  lv_obj_center(widgets.scrim);

  auto* panel = lv_obj_create(widgets.scrim);
  if (!panel) {
    widgets.close();
    return {};
  }
  theme.apply_dialog(panel);
  lv_obj_set_size(panel, panel_width(screen), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(panel, 28, 0);
  lv_obj_set_style_pad_row(panel, 18, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_center(panel);
  if (!overlay_label(panel, spec.title, theme, TextRole::kTitle) ||
      (spec.body.text && !overlay_label(panel, spec.body, theme, TextRole::kMuted))) {
    widgets.close();
    return {};
  }

  auto* actions = lv_obj_create(panel);
  if (!actions) {
    widgets.close();
    return {};
  }
  lv_obj_remove_style_all(actions);
  lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(actions, LV_PCT(100), 52);
  lv_obj_set_style_pad_column(actions, 12, 0);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
  if (spec.cancel.text) {
    widgets.cancel = confirmation_button(actions, spec.cancel, theme, ButtonVariant::kSecondary,
                                          spec.on_cancel, spec.context);
  }
  widgets.confirm = confirmation_button(actions, spec.confirm, theme, ButtonVariant::kPrimary,
                                         spec.on_confirm, spec.context);
  if (!widgets.confirm || (spec.cancel.text && !widgets.cancel)) widgets.close();
  return widgets;
}

bool Toast::show(lv_obj_t* anchor, UiTheme& theme, OverlayText message,
                 std::uint32_t duration_ms, OnExpired on_expired, void* context) noexcept {
  dismiss();
  if (!anchor || !message.text || duration_ms == 0U) return false;
  auto* screen = lv_obj_get_screen(anchor);
  if (panel_width(screen) <= 0) return false;
  panel_ = lv_obj_create(screen);
  if (!panel_) return false;
  theme.apply_toast(panel_);
  lv_obj_add_flag(panel_, LV_OBJ_FLAG_FLOATING);
  lv_obj_clear_flag(panel_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(panel_, panel_width(screen), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(panel_, 20, 0);
  lv_obj_set_style_pad_ver(panel_, 15, 0);
  lv_obj_align(panel_, LV_ALIGN_BOTTOM_MID, 0, -18);
  auto* text = overlay_label(panel_, message, theme, TextRole::kInverse);
  if (text) {
    lv_obj_center(text);
    timer_ = lv_timer_create(expire, duration_ms, this);
  }
  if (!timer_) {
    dismiss();
    return false;
  }
  on_expired_ = on_expired;
  context_ = context;
  return true;
}

void Toast::dismiss() noexcept {
  if (timer_) lv_timer_del(std::exchange(timer_, nullptr));
  if (panel_) lv_obj_del(std::exchange(panel_, nullptr));
  on_expired_ = nullptr;
  context_ = nullptr;
}

void Toast::expire(lv_timer_t* timer) {
  auto& toast = *static_cast<Toast*>(timer->user_data);
  const auto callback = toast.on_expired_;
  auto* context = toast.context_;
  toast.dismiss();
  if (callback) callback(context);
}

}  // namespace hmi::ui
