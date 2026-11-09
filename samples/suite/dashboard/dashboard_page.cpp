#include "dashboard/dashboard_page.hpp"

#include "hmi/ui/components.hpp"
#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"

namespace hmi::samples::suite {
namespace {

namespace generated = hmi::samples::suite::generated;

void apply_fixed_font(lv_obj_t *label,
                      generated::TypographyRole role) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(label, generated::fixed_font(role), 0);
  }
}

lv_obj_t *create_fixed_label(lv_obj_t *parent, hmi::i18n::Locale locale,
                             generated::TextId id, hmi::ui::UiTheme &theme,
                             hmi::ui::TextRole role) noexcept {
  lv_obj_t *label = hmi::ui::create_label(
      parent, generated::text(locale, id).data(), theme, role);
  apply_fixed_font(label, generated::typography(id));
  return label;
}

lv_obj_t *create_fixed_button(lv_obj_t *parent, hmi::i18n::Locale locale,
                              generated::TextId id, hmi::ui::UiTheme &theme,
                              hmi::ui::ButtonVariant variant,
                              lv_event_cb_t callback, void *user_data) noexcept {
  lv_obj_t *button = hmi::ui::create_button(
      parent, generated::text(locale, id).data(), theme, variant, callback,
      user_data);
  if (button != nullptr) {
    apply_fixed_font(lv_obj_get_child(button, 0), generated::typography(id));
  }
  return button;
}

}  // namespace

bool DashboardPage::create(lv_obj_t *parent,
                           const PageDescriptor *descriptors,
                           std::size_t descriptor_count,
                           SelectCallback on_select,
                           ThemeCallback on_theme_toggle,
                           LocaleCallback on_locale_change,
                           hmi::i18n::Locale locale, hmi::ui::UiTheme &theme,
                           void *user_data) {
  if (root_ != nullptr || parent == nullptr || descriptors == nullptr ||
      descriptor_count == 0U || on_select == nullptr ||
      on_theme_toggle == nullptr || on_locale_change == nullptr) {
    return false;
  }

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  on_select_ = on_select;
  on_theme_toggle_ = on_theme_toggle;
  on_locale_change_ = on_locale_change;
  user_data_ = user_data;
  theme.apply_screen(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(root_, 24, 0);
  lv_obj_set_style_pad_row(root_, 18, 0);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *header = lv_obj_create(root_);
  if (header == nullptr) {
    destroy();
    return false;
  }
  theme.apply_header(header);
  lv_obj_set_size(header, LV_PCT(100), 130);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(header, 20, 0);
  lv_obj_set_style_pad_column(header, 18, 0);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *brand = lv_obj_create(header);
  if (brand == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(brand);
  lv_obj_set_height(brand, LV_PCT(100));
  lv_obj_set_flex_grow(brand, 1);
  lv_obj_clear_flag(brand, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(brand, 0, 0);
  lv_obj_set_style_pad_row(brand, 5, 0);
  lv_obj_set_flex_flow(brand, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(brand, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  lv_obj_t *eyebrow = create_fixed_label(
      brand, locale, generated::TextId::dashboard_eyebrow, theme,
      hmi::ui::TextRole::kAccent);
  lv_obj_t *title = create_fixed_label(
      brand, locale, generated::TextId::dashboard_title, theme,
      hmi::ui::TextRole::kDisplay);
  lv_obj_t *subtitle = create_fixed_label(
      brand, locale, generated::TextId::dashboard_description, theme,
      hmi::ui::TextRole::kMuted);
  if (eyebrow == nullptr || title == nullptr || subtitle == nullptr) {
    destroy();
    return false;
  }
  hmi::ui::configure_multiline(subtitle, LV_PCT(100), 24, 3);

  lv_obj_t *controls = lv_obj_create(header);
  if (controls == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(controls);
  lv_obj_set_size(controls, 310, 42);
  lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(controls, 0, 0);
  lv_obj_set_style_pad_column(controls, 8, 0);
  lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  locale_ko_button_ = create_fixed_button(
      controls, locale, generated::TextId::locale_ko, theme,
      locale == hmi::i18n::Locale::ko_kr ? hmi::ui::ButtonVariant::kPrimary
                                         : hmi::ui::ButtonVariant::kGhost,
      &DashboardPage::handle_locale_event, this);
  locale_en_button_ = create_fixed_button(
      controls, locale, generated::TextId::locale_en, theme,
      locale == hmi::i18n::Locale::en_us ? hmi::ui::ButtonVariant::kPrimary
                                         : hmi::ui::ButtonVariant::kGhost,
      &DashboardPage::handle_locale_event, this);
  if (locale_ko_button_ == nullptr || locale_en_button_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(locale_ko_button_, 92, 42);
  lv_obj_set_size(locale_en_button_, 92, 42);

  const generated::TextId theme_id =
      theme.mode() == hmi::ui::ThemeMode::kDark
          ? generated::TextId::theme_light
          : generated::TextId::theme_dark;
  lv_obj_t *theme_button = create_fixed_button(
      controls, locale, theme_id, theme, hmi::ui::ButtonVariant::kGhost,
      &DashboardPage::handle_theme_event, this);
  if (theme_button == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(theme_button, 110, 42);

  lv_obj_t *main = lv_obj_create(root_);
  if (main == nullptr) {
    destroy();
    return false;
  }
  theme.apply_surface(main);
  lv_obj_set_width(main, LV_PCT(100));
  lv_obj_set_flex_grow(main, 1);
  lv_obj_clear_flag(main, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(main, 22, 0);
  lv_obj_set_style_pad_row(main, 14, 0);
  lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *grid = lv_obj_create(main);
  if (grid == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(grid);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_row(grid, 14, 0);
  lv_obj_set_style_pad_column(grid, 14, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);

  for (std::size_t index = 0; index < descriptor_count; ++index) {
    const PageDescriptor &descriptor = descriptors[index];
    lv_obj_t *card = lv_btn_create(grid);
    if (card == nullptr) {
      destroy();
      return false;
    }
    theme.apply_card(card);
    // 32% 카드 세 개와 14px 간격 두 개가 한 행에 안정적으로 들어간다.
    // 높이는 번역된 설명의 줄 수에 맞춰 불필요한 하단 공백을 남기지 않는다.
    lv_obj_set_size(card, LV_PCT(32), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_user_data(card, const_cast<PageDescriptor *>(&descriptor));
    lv_obj_add_event_cb(card, &DashboardPage::handle_page_event,
                        LV_EVENT_SHORT_CLICKED, this);

    lv_obj_t *card_title = hmi::ui::create_label(
        card, generated::text(locale, descriptor.title_key).data(), theme,
        hmi::ui::TextRole::kTitle);
    lv_obj_t *description = hmi::ui::create_label(
        card, generated::text(locale, descriptor.description_key).data(), theme,
        hmi::ui::TextRole::kMuted);
    if (card_title == nullptr || description == nullptr) {
      destroy();
      return false;
    }
    apply_fixed_font(card_title, generated::typography(descriptor.title_key));
    apply_fixed_font(description,
                     generated::typography(descriptor.description_key));
    hmi::ui::configure_multiline(description, LV_PCT(100), LV_SIZE_CONTENT, 5);

  }
  return true;
}

void DashboardPage::handle_page_event(lv_event_t *event) {
  if (event == nullptr) {
    return;
  }
  auto *self = static_cast<DashboardPage *>(lv_event_get_user_data(event));
  auto *descriptor = static_cast<const PageDescriptor *>(
      lv_obj_get_user_data(lv_event_get_target(event)));
  if (self != nullptr && descriptor != nullptr && self->on_select_ != nullptr) {
    self->on_select_(self->user_data_, descriptor->id);
  }
}

void DashboardPage::handle_theme_event(lv_event_t *event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto *self = static_cast<DashboardPage *>(lv_event_get_user_data(event));
  if (self != nullptr && self->on_theme_toggle_ != nullptr) {
    self->on_theme_toggle_(self->user_data_);
  }
}

void DashboardPage::handle_locale_event(lv_event_t *event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto *self = static_cast<DashboardPage *>(lv_event_get_user_data(event));
  if (self == nullptr || self->on_locale_change_ == nullptr) {
    return;
  }
  lv_obj_t *target = lv_event_get_target(event);
  if (target == self->locale_ko_button_) {
    self->on_locale_change_(self->user_data_, hmi::i18n::Locale::ko_kr);
  } else if (target == self->locale_en_button_) {
    self->on_locale_change_(self->user_data_, hmi::i18n::Locale::en_us);
  }
}

void DashboardPage::destroy() noexcept {
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  on_select_ = nullptr;
  on_theme_toggle_ = nullptr;
  on_locale_change_ = nullptr;
  locale_ko_button_ = nullptr;
  locale_en_button_ = nullptr;
  user_data_ = nullptr;
}

DashboardPage::~DashboardPage() { destroy(); }

}  // namespace hmi::samples::suite
