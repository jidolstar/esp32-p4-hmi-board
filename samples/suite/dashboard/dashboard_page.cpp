#include "dashboard/dashboard_page.hpp"

#include "components/page_header.hpp"
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
  configure_page_shell_root(root_, theme);

  PageHeaderSpec header_spec;
  header_spec.title = generated::TextId::dashboard_title;
  header_spec.locale = locale;
  header_spec.title_leading_inset = 10;
  header_spec.locale_ko_event = &DashboardPage::handle_locale_event;
  header_spec.locale_en_event = &DashboardPage::handle_locale_event;
  header_spec.theme_event = &DashboardPage::handle_theme_event;
  header_spec.user_data = this;
  const PageHeaderWidgets header =
      create_page_header(root_, header_spec, theme);
  if (!header) {
    destroy();
    return false;
  }
  locale_ko_button_ = header.locale_ko_button;
  locale_en_button_ = header.locale_en_button;

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
        card, generated::text(locale, descriptor.dashboard_title_key).data(), theme,
        hmi::ui::TextRole::kTitle);
    lv_obj_t *description = hmi::ui::create_label(
        card, generated::text(locale, descriptor.description_key).data(), theme,
        hmi::ui::TextRole::kMuted);
    if (card_title == nullptr || description == nullptr) {
      destroy();
      return false;
    }
    apply_fixed_font(
        card_title, generated::typography(descriptor.dashboard_title_key));
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
