#include "components/page_header.hpp"

#include "generated/fixed_fonts.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {

void apply_fixed_font(lv_obj_t* label, const generated::TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

lv_obj_t* create_label(lv_obj_t* parent, const PageHeaderSpec& spec,
                       const generated::TextId id,
                       hmi::ui::UiTheme& theme,
                       const hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(spec.locale, id).data(), theme, role);
  apply_fixed_font(label, id);
  return label;
}

lv_obj_t* create_button(lv_obj_t* parent, const PageHeaderSpec& spec,
                        const generated::TextId id,
                        hmi::ui::UiTheme& theme,
                        const hmi::ui::ButtonVariant variant,
                        const lv_event_cb_t callback) noexcept {
  lv_obj_t* button = hmi::ui::create_button(
      parent, generated::text(spec.locale, id).data(), theme, variant,
      callback, spec.user_data);
  if (button != nullptr) {
    apply_fixed_font(lv_obj_get_child(button, 0), id);
  }
  return button;
}

lv_obj_t* create_badge(lv_obj_t* parent, const PageHeaderSpec& spec,
                       hmi::ui::UiTheme& theme) noexcept {
  if (spec.badge == generated::TextId::count) {
    return nullptr;
  }
  lv_obj_t* badge = lv_obj_create(parent);
  if (badge == nullptr) {
    return nullptr;
  }
  theme.apply_chip(badge);
  lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(badge, LV_SIZE_CONTENT, 34);
  lv_obj_set_style_pad_hor(badge, 12, 0);
  lv_obj_set_style_pad_ver(badge, 6, 0);
  lv_obj_t* label = create_label(badge, spec, spec.badge, theme,
                                 hmi::ui::TextRole::kAccent);
  if (label == nullptr) {
    lv_obj_del(badge);
    return nullptr;
  }
  lv_obj_center(label);
  return badge;
}

}  // namespace

PageHeaderWidgets create_page_header(lv_obj_t* parent,
                                     const PageHeaderSpec& spec,
                                     hmi::ui::UiTheme& theme) noexcept {
  PageHeaderWidgets widgets;
  if (parent == nullptr || spec.title == generated::TextId::count ||
      spec.locale_ko_event == nullptr || spec.locale_en_event == nullptr ||
      spec.theme_event == nullptr) {
    return widgets;
  }

  widgets.root = lv_obj_create(parent);
  if (widgets.root == nullptr) {
    return {};
  }
  theme.apply_header(widgets.root);
  lv_obj_set_size(widgets.root, LV_PCT(100), kPageHeaderHeight);
  lv_obj_clear_flag(widgets.root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(widgets.root, 8, 0);
  lv_obj_set_style_pad_column(widgets.root, 10, 0);
  lv_obj_set_flex_flow(widgets.root, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(widgets.root, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  if (spec.back_event != nullptr) {
    widgets.back_button = create_button(
        widgets.root, spec, generated::TextId::common_back, theme,
        hmi::ui::ButtonVariant::kGhost, spec.back_event);
    if (widgets.back_button == nullptr) {
      lv_obj_del(widgets.root);
      return {};
    }
    lv_obj_set_size(widgets.back_button, 84, 42);
  }

  lv_obj_t* title = create_label(widgets.root, spec, spec.title, theme,
                                 hmi::ui::TextRole::kTitle);
  if (title != nullptr && spec.title_leading_inset > 0) {
    lv_obj_set_style_translate_x(title, spec.title_leading_inset, 0);
  }
  lv_obj_t* badge = create_badge(widgets.root, spec, theme);
  lv_obj_t* spacer = lv_obj_create(widgets.root);
  widgets.locale_ko_button = create_button(
      widgets.root, spec, generated::TextId::locale_ko, theme,
      spec.locale == hmi::i18n::Locale::ko_kr
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      spec.locale_ko_event);
  widgets.locale_en_button = create_button(
      widgets.root, spec, generated::TextId::locale_en, theme,
      spec.locale == hmi::i18n::Locale::en_us
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      spec.locale_en_event);
  const generated::TextId theme_id =
      theme.mode() == hmi::ui::ThemeMode::kDark
          ? generated::TextId::theme_light
          : generated::TextId::theme_dark;
  widgets.theme_button = create_button(
      widgets.root, spec, theme_id, theme, hmi::ui::ButtonVariant::kGhost,
      spec.theme_event);
  if (title == nullptr ||
      (spec.badge != generated::TextId::count && badge == nullptr) ||
      spacer == nullptr || widgets.locale_ko_button == nullptr ||
      widgets.locale_en_button == nullptr || widgets.theme_button == nullptr) {
    lv_obj_del(widgets.root);
    return {};
  }

  theme.apply_transparent(spacer);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_size(widgets.locale_ko_button, 88, 42);
  lv_obj_set_size(widgets.locale_en_button, 88, 42);
  lv_obj_set_size(widgets.theme_button, 104, 42);
  return widgets;
}

void configure_page_shell_root(lv_obj_t* root,
                               hmi::ui::UiTheme& theme) noexcept {
  if (root == nullptr) {
    return;
  }
  theme.apply_screen(root);
  lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(root, kPageOuterPadding, 0);
  lv_obj_set_style_pad_row(root, kPageSectionGap, 0);
  lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
}

}  // namespace hmi::samples::suite
