#include "pages/interfaces_info/interfaces_info_page.hpp"

#include "components/page_header.hpp"
#include "generated/fixed_fonts.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {

using generated::TextId;

void apply_fixed_font(lv_obj_t* label, TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

lv_obj_t* create_transparent(lv_obj_t* parent, hmi::ui::UiTheme& theme,
                             lv_flex_flow_t flow) noexcept {
  lv_obj_t* value = lv_obj_create(parent);
  if (value == nullptr) {
    return nullptr;
  }
  theme.apply_transparent(value);
  lv_obj_clear_flag(value, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(value, 0, 0);
  lv_obj_set_flex_flow(value, flow);
  return value;
}

}  // namespace

InterfacesInfoPage::~InterfacesInfoPage() { destroy(); }

bool InterfacesInfoPage::create(lv_obj_t* parent,
                                const PageContext& context) {
  if (parent == nullptr || root_ != nullptr || context.theme == nullptr ||
      !context.theme->initialized() || context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr) {
    return false;
  }
  context_ = context;
  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    destroy();
    return false;
  }
  configure_page_shell_root(root_, *context_.theme);
  if (!create_header(root_)) {
    destroy();
    return false;
  }
  body_ = create_transparent(root_, *context_.theme, LV_FLEX_FLOW_COLUMN);
  if (body_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_width(body_, LV_PCT(100));
  lv_obj_set_flex_grow(body_, 1);
  if (!render_body()) {
    destroy();
    return false;
  }
  return true;
}

bool InterfacesInfoPage::create_header(lv_obj_t* parent) noexcept {
  PageHeaderSpec spec;
  spec.title = TextId::page_interfaces_info_title;
  spec.badge = TextId::interfaces_badge;
  spec.locale = context_.locale;
  spec.back_event = &InterfacesInfoPage::handle_event;
  spec.locale_ko_event = &InterfacesInfoPage::handle_event;
  spec.locale_en_event = &InterfacesInfoPage::handle_event;
  spec.theme_event = &InterfacesInfoPage::handle_event;
  spec.user_data = this;
  const auto header = create_page_header(parent, spec, *context_.theme);
  if (!header) {
    return false;
  }
  back_button_ = header.back_button;
  locale_ko_button_ = header.locale_ko_button;
  locale_en_button_ = header.locale_en_button;
  theme_button_ = header.theme_button;
  return true;
}

lv_obj_t* InterfacesInfoPage::create_label(
    lv_obj_t* parent, TextId id, hmi::ui::TextRole role) noexcept {
  lv_obj_t* label = hmi::ui::create_label(
      parent, generated::text(context_.locale, id).data(), *context_.theme,
      role);
  apply_fixed_font(label, id);
  return label;
}

bool InterfacesInfoPage::render_body() noexcept {
  if (body_ == nullptr) {
    return false;
  }
  lv_obj_clean(body_);
  rows_.fill(nullptr);
  return model_.view() == InterfacesInfoView::list ? render_list()
                                                   : render_detail();
}

bool InterfacesInfoPage::render_list() noexcept {
  auto& theme = *context_.theme;
  lv_obj_t* intro = lv_obj_create(body_);
  lv_obj_t* list = lv_obj_create(body_);
  if (intro == nullptr || list == nullptr) {
    return false;
  }
  theme.apply_status(intro);
  lv_obj_set_size(intro, LV_PCT(100), 62);
  lv_obj_clear_flag(intro, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(intro, 12, 0);
  lv_obj_t* hint = create_label(intro, TextId::interfaces_list_hint,
                                hmi::ui::TextRole::kBody);
  if (hint == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(hint, 930, 38, 5, LV_TEXT_ALIGN_LEFT);
  lv_obj_center(hint);

  theme.apply_surface(list);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_style_pad_all(list, 12, 0);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

  const auto& topics = interface_topic_descriptors();
  for (std::size_t index = 0U; index < topics.size(); ++index) {
    const auto& topic = topics[index];
    lv_obj_t* row = lv_obj_create(list);
    if (row == nullptr) {
      return false;
    }
    rows_[index] = row;
    theme.apply_list_item(row);
    lv_obj_set_size(row, LV_PCT(100), 88);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(row, &InterfacesInfoPage::handle_event,
                        LV_EVENT_ALL, this);

    lv_obj_t* text = create_transparent(row, theme, LV_FLEX_FLOW_COLUMN);
    lv_obj_t* chip = lv_obj_create(row);
    if (text == nullptr || chip == nullptr) {
      return false;
    }
    lv_obj_clear_flag(text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(text, 0);
    lv_obj_set_height(text, LV_PCT(100));
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_style_pad_row(text, 4, 0);
    lv_obj_t* title = create_label(text, topic.title,
                                   hmi::ui::TextRole::kTitle);
    lv_obj_t* summary = create_label(text, topic.summary,
                                     hmi::ui::TextRole::kMuted);
    if (title == nullptr || summary == nullptr) {
      return false;
    }
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(summary, LV_OBJ_FLAG_CLICKABLE);
    hmi::ui::configure_multiline(summary, 720, 38, 5,
                                 LV_TEXT_ALIGN_LEFT);

    theme.apply_chip(chip);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(chip, 148, 38);
    lv_obj_set_style_pad_all(chip, 6, 0);
    lv_obj_t* status = create_label(chip, topic.status,
                                    hmi::ui::TextRole::kAccent);
    if (status == nullptr) {
      return false;
    }
    lv_obj_clear_flag(status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(status);
  }
  return true;
}

bool InterfacesInfoPage::create_detail_section(
    lv_obj_t* parent, TextId heading, TextId body) noexcept {
  lv_obj_t* section = lv_obj_create(parent);
  if (section == nullptr) {
    return false;
  }
  context_.theme->apply_card(section);
  lv_obj_set_size(section, LV_PCT(100), 122);
  lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(section, 12, 0);
  lv_obj_set_style_pad_row(section, 6, 0);
  lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
  lv_obj_t* title = create_label(section, heading,
                                 hmi::ui::TextRole::kAccent);
  lv_obj_t* paragraph = create_label(section, body,
                                     hmi::ui::TextRole::kBody);
  if (title == nullptr || paragraph == nullptr) {
    return false;
  }
  hmi::ui::configure_multiline(paragraph, 920, 66, 5,
                               LV_TEXT_ALIGN_LEFT);
  return true;
}

bool InterfacesInfoPage::render_detail() noexcept {
  auto& theme = *context_.theme;
  const auto& topic = model_.selected();
  lv_obj_t* scroll = lv_obj_create(body_);
  if (scroll == nullptr) {
    return false;
  }
  theme.apply_surface(scroll);
  lv_obj_set_width(scroll, LV_PCT(100));
  lv_obj_set_flex_grow(scroll, 1);
  lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
  lv_obj_set_style_pad_all(scroll, 14, 0);
  lv_obj_set_style_pad_row(scroll, 10, 0);
  lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* heading = create_transparent(scroll, theme, LV_FLEX_FLOW_ROW);
  if (heading == nullptr) {
    return false;
  }
  lv_obj_set_size(heading, LV_PCT(100), 52);
  lv_obj_set_flex_align(heading, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_t* title = create_label(heading, topic.title,
                                 hmi::ui::TextRole::kDisplay);
  lv_obj_t* spacer = lv_obj_create(heading);
  lv_obj_t* chip = lv_obj_create(heading);
  if (title == nullptr || spacer == nullptr || chip == nullptr) {
    return false;
  }
  theme.apply_transparent(spacer);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  theme.apply_chip(chip);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(chip, 160, 38);
  lv_obj_t* status = create_label(chip, topic.status,
                                  hmi::ui::TextRole::kAccent);
  if (status == nullptr) {
    return false;
  }
  lv_obj_center(status);

  return create_detail_section(scroll, TextId::interfaces_section_purpose,
                               topic.purpose) &&
         create_detail_section(scroll, TextId::interfaces_section_provided,
                               topic.provided) &&
         create_detail_section(scroll, TextId::interfaces_section_connect,
                               topic.connect) &&
         create_detail_section(scroll, TextId::interfaces_section_steps,
                               topic.steps) &&
         create_detail_section(scroll, TextId::interfaces_section_limits,
                               topic.limits) &&
         create_detail_section(scroll, TextId::interfaces_section_excluded,
                               topic.excluded);
}

void InterfacesInfoPage::handle_event(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<InterfacesInfoPage*>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  lv_obj_t* target = lv_event_get_target(event);
  if (target == self->back_button_) {
    if (self->model_.back_to_list()) {
      static_cast<void>(self->render_body());
    } else {
      self->context_.request_dashboard(self->context_.user_data);
    }
    return;
  }
  if (target == self->theme_button_) {
    self->context_.request_theme_toggle(self->context_.user_data);
    return;
  }
  if (target == self->locale_ko_button_) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::ko_kr);
    return;
  }
  if (target == self->locale_en_button_) {
    self->context_.set_locale(self->context_.user_data,
                              hmi::i18n::Locale::en_us);
    return;
  }
  for (std::size_t index = 0U; index < self->rows_.size(); ++index) {
    if (target == self->rows_[index] && self->model_.open(index)) {
      static_cast<void>(self->render_body());
      return;
    }
  }
}

void InterfacesInfoPage::reset_widgets() noexcept {
  root_ = nullptr;
  body_ = nullptr;
  back_button_ = nullptr;
  locale_ko_button_ = nullptr;
  locale_en_button_ = nullptr;
  theme_button_ = nullptr;
  rows_.fill(nullptr);
}

void InterfacesInfoPage::destroy() noexcept {
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  reset_widgets();
  context_ = {};
}

}  // namespace hmi::samples::suite
