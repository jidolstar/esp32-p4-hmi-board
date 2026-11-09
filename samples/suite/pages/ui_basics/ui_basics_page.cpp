#include "pages/ui_basics/ui_basics_page.hpp"

#include <array>
#include <limits>

#include "hmi/i18n/format.hpp"
#include "hmi/ui/components.hpp"
#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"

namespace hmi::samples::suite {
static_assert(sizeof(UiBasicsPage) <= 256U,
              "UI Basics controller must not retain a large embedded model");
namespace {

struct ListItem {
  const char *number;
  generated::TextId name;
};

constexpr std::array<ListItem, 10> kListItems = {{
    {"01", generated::TextId::ui_basics_list_item_alpha},
    {"02", generated::TextId::ui_basics_list_item_bravo},
    {"03", generated::TextId::ui_basics_list_item_charlie},
    {"04", generated::TextId::ui_basics_list_item_delta},
    {"05", generated::TextId::ui_basics_list_item_echo},
    {"06", generated::TextId::ui_basics_list_item_foxtrot},
    {"07", generated::TextId::ui_basics_list_item_golf},
    {"08", generated::TextId::ui_basics_list_item_hotel},
    {"09", generated::TextId::ui_basics_list_item_india},
    {"10", generated::TextId::ui_basics_list_item_juliett},
}};

void apply_fixed_font(lv_obj_t *label, generated::TextId id) noexcept {
  if (label != nullptr) {
    lv_obj_set_style_text_font(
        label, generated::fixed_font(generated::typography(id)), 0);
  }
}

lv_obj_t *create_fixed_label(lv_obj_t *parent, hmi::i18n::Locale locale,
                             generated::TextId id, hmi::ui::UiTheme &theme,
                             hmi::ui::TextRole role) noexcept {
  lv_obj_t *label = hmi::ui::create_label(
      parent, generated::text(locale, id).data(), theme, role);
  apply_fixed_font(label, id);
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
    apply_fixed_font(lv_obj_get_child(button, 0), id);
  }
  return button;
}

lv_obj_t *create_chip(lv_obj_t *parent, const char *text,
                      hmi::ui::UiTheme &theme,
                      generated::TextId fixed_id = generated::TextId::count,
                      hmi::i18n::Locale locale = hmi::i18n::Locale::ko_kr) noexcept {
  lv_obj_t *chip = lv_obj_create(parent);
  if (chip == nullptr) {
    return nullptr;
  }
  theme.apply_chip(chip);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(chip, LV_SIZE_CONTENT);
  lv_obj_set_height(chip, 34);
  lv_obj_set_style_pad_hor(chip, 12, 0);
  lv_obj_set_style_pad_ver(chip, 6, 0);
  const char *caption = fixed_id == generated::TextId::count
                            ? text
                            : generated::text(locale, fixed_id).data();
  lv_obj_t *label = hmi::ui::create_label(
      chip, caption, theme, hmi::ui::TextRole::kAccent);
  if (label == nullptr) {
    lv_obj_del(chip);
    return nullptr;
  }
  if (fixed_id != generated::TextId::count) {
    apply_fixed_font(label, fixed_id);
  }
  lv_obj_center(label);
  return chip;
}

void configure_row(lv_obj_t *row, hmi::ui::UiTheme &theme) noexcept {
  theme.apply_transparent(row);
  lv_obj_set_size(row, LV_PCT(100), 58);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
}

}  // namespace

bool UiBasicsPage::create(lv_obj_t *parent, const PageContext &context) {
  if (root_ != nullptr || parent == nullptr ||
      context.request_dashboard == nullptr ||
      context.request_theme_toggle == nullptr || context.set_locale == nullptr ||
      context.theme == nullptr ||
      !context.theme->initialized()) {
    return false;
  }
  context_ = context;
  hmi::ui::UiTheme &theme = *context_.theme;

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    return false;
  }
  theme.apply_screen(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(root_, 18, 0);
  lv_obj_set_style_pad_row(root_, 14, 0);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *header = lv_obj_create(root_);
  if (header == nullptr) {
    destroy();
    return false;
  }
  theme.apply_header(header);
  lv_obj_set_size(header, LV_PCT(100), 74);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(header, 12, 0);
  lv_obj_set_style_pad_column(header, 12, 0);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  back_button_ = create_fixed_button(
      header, context_.locale, generated::TextId::common_back, theme,
      hmi::ui::ButtonVariant::kGhost,
      &UiBasicsPage::handle_event, this);
  lv_obj_t *title = create_fixed_label(
      header, context_.locale, generated::TextId::page_ui_basics_title, theme,
      hmi::ui::TextRole::kDisplay);
  lv_obj_t *badge = create_chip(
      header, nullptr, theme, generated::TextId::ui_basics_badge,
      context_.locale);
  lv_obj_t *spacer = lv_obj_create(header);
  locale_ko_button_ = create_fixed_button(
      header, context_.locale, generated::TextId::locale_ko, theme,
      context_.locale == hmi::i18n::Locale::ko_kr
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &UiBasicsPage::handle_event, this);
  locale_en_button_ = create_fixed_button(
      header, context_.locale, generated::TextId::locale_en, theme,
      context_.locale == hmi::i18n::Locale::en_us
          ? hmi::ui::ButtonVariant::kPrimary
          : hmi::ui::ButtonVariant::kGhost,
      &UiBasicsPage::handle_event, this);
  const generated::TextId theme_id =
      theme.mode() == hmi::ui::ThemeMode::kDark
          ? generated::TextId::theme_light
          : generated::TextId::theme_dark;
  theme_button_ = create_fixed_button(
      header, context_.locale, theme_id, theme,
      hmi::ui::ButtonVariant::kGhost,
      &UiBasicsPage::handle_event, this);
  if (back_button_ == nullptr || title == nullptr || badge == nullptr ||
      spacer == nullptr || locale_ko_button_ == nullptr ||
      locale_en_button_ == nullptr || theme_button_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(back_button_, 92, 46);
  theme.apply_transparent(spacer);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_size(locale_ko_button_, 92, 42);
  lv_obj_set_size(locale_en_button_, 92, 42);
  lv_obj_set_size(theme_button_, 110, 42);

  lv_obj_t *body = lv_obj_create(root_);
  if (body == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(body);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_style_pad_column(body, 14, 0);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

  lv_obj_t *controls = lv_obj_create(body);
  lv_obj_t *list_panel = lv_obj_create(body);
  if (controls == nullptr || list_panel == nullptr) {
    destroy();
    return false;
  }
  theme.apply_surface(controls);
  lv_obj_set_width(controls, 474);
  lv_obj_set_height(controls, LV_PCT(100));
  lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(controls, 18, 0);
  lv_obj_set_style_pad_row(controls, 12, 0);
  lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *control_heading = create_fixed_label(
      controls, context_.locale, generated::TextId::ui_basics_controls_title,
      theme, hmi::ui::TextRole::kTitle);
  lv_obj_t *instruction = create_fixed_label(
      controls, context_.locale,
      generated::TextId::ui_basics_controls_instruction, theme,
      hmi::ui::TextRole::kMuted);
  if (control_heading == nullptr || instruction == nullptr) {
    destroy();
    return false;
  }
  hmi::ui::configure_multiline(instruction, LV_PCT(100), 48, 4);

  lv_obj_t *button_row = lv_obj_create(controls);
  lv_obj_t *overlay_row = lv_obj_create(controls);
  if (button_row == nullptr || overlay_row == nullptr) {
    destroy();
    return false;
  }
  configure_row(button_row, theme);
  configure_row(overlay_row, theme);

  press_button_ = create_fixed_button(
      button_row, context_.locale, generated::TextId::ui_basics_action_press,
      theme, hmi::ui::ButtonVariant::kPrimary,
      &UiBasicsPage::handle_event, this);
  hold_button_ = create_fixed_button(
      button_row, context_.locale, generated::TextId::ui_basics_action_hold,
      theme, hmi::ui::ButtonVariant::kSecondary,
      &UiBasicsPage::handle_event, this);
  toast_button_ = create_fixed_button(
      overlay_row, context_.locale,
      generated::TextId::ui_basics_action_show_toast, theme,
      hmi::ui::ButtonVariant::kGhost,
      &UiBasicsPage::handle_event, this);
  dialog_button_ = create_fixed_button(
      overlay_row, context_.locale,
      generated::TextId::ui_basics_action_open_dialog, theme,
      hmi::ui::ButtonVariant::kPrimary,
      &UiBasicsPage::handle_event, this);
  if (press_button_ == nullptr || hold_button_ == nullptr ||
      toast_button_ == nullptr || dialog_button_ == nullptr) {
    destroy();
    return false;
  }
  for (lv_obj_t *button : {press_button_, hold_button_, toast_button_,
                           dialog_button_}) {
    lv_obj_set_size(button, 209, 52);
  }

  lv_obj_t *status = lv_obj_create(controls);
  if (status == nullptr) {
    destroy();
    return false;
  }
  theme.apply_status(status);
  lv_obj_set_size(status, LV_PCT(100), 64);
  lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(status, 14, 0);
  status_label_ = create_fixed_label(
      status, context_.locale, generated::TextId::ui_basics_status_summary,
      theme, hmi::ui::TextRole::kBody);
  if (status_label_ == nullptr) {
    destroy();
    return false;
  }
  hmi::ui::configure_multiline(status_label_, LV_PCT(100), 36, 3);
  lv_obj_center(status_label_);

  theme.apply_surface(list_panel);
  lv_obj_set_width(list_panel, 500);
  lv_obj_set_height(list_panel, LV_PCT(100));
  lv_obj_set_scroll_dir(list_panel, LV_DIR_VER);
  lv_obj_set_style_pad_all(list_panel, 16, 0);
  lv_obj_set_style_pad_row(list_panel, 10, 0);
  lv_obj_set_flex_flow(list_panel, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *list_header = lv_obj_create(list_panel);
  if (list_header == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(list_header);
  lv_obj_set_size(list_header, LV_PCT(100), 42);
  lv_obj_clear_flag(list_header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(list_header, 0, 0);
  lv_obj_set_flex_flow(list_header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(list_header, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *list_title = create_fixed_label(
      list_header, context_.locale, generated::TextId::ui_basics_list_title,
      theme, hmi::ui::TextRole::kTitle);
  lv_obj_t *list_spacer = lv_obj_create(list_header);
  lv_obj_t *count = create_chip(
      list_header, nullptr, theme, generated::TextId::ui_basics_list_count,
      context_.locale);
  if (list_title == nullptr || list_spacer == nullptr || count == nullptr) {
    destroy();
    return false;
  }
  theme.apply_transparent(list_spacer);
  lv_obj_set_height(list_spacer, 1);
  lv_obj_set_flex_grow(list_spacer, 1);

  for (const ListItem &item : kListItems) {
    lv_obj_t *row = lv_obj_create(list_panel);
    if (row == nullptr) {
      destroy();
      return false;
    }
    theme.apply_list_item(row);
    lv_obj_set_size(row, LV_PCT(100), 56);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t *number = create_chip(row, item.number, theme);
    lv_obj_t *name = create_fixed_label(
        row, context_.locale, item.name, theme, hmi::ui::TextRole::kBody);
    if (number == nullptr || name == nullptr) {
      destroy();
      return false;
    }
    lv_obj_set_width(number, 48);
  }
  update_status();
  return true;
}

void UiBasicsPage::handle_event(lv_event_t *event) {
  if (event == nullptr) {
    return;
  }
  auto *self = static_cast<UiBasicsPage *>(lv_event_get_user_data(event));
  if (self == nullptr) {
    return;
  }
  const lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *target = lv_event_get_target(event);
  if (target == self->hold_button_ && code == LV_EVENT_LONG_PRESSED) {
    self->record_long_press();
  } else if (code == LV_EVENT_SHORT_CLICKED) {
    if (target == self->back_button_) {
      self->context_.request_dashboard(self->context_.user_data);
    } else if (target == self->locale_ko_button_) {
      self->context_.set_locale(self->context_.user_data,
                                hmi::i18n::Locale::ko_kr);
    } else if (target == self->locale_en_button_) {
      self->context_.set_locale(self->context_.user_data,
                                hmi::i18n::Locale::en_us);
    } else if (target == self->theme_button_) {
      self->context_.request_theme_toggle(self->context_.user_data);
    } else if (target == self->press_button_ || target == self->hold_button_) {
      self->record_press();
    } else if (target == self->toast_button_) {
      self->show_toast();
    } else if (target == self->dialog_button_) {
      self->open_dialog();
    } else if (target == self->dialog_cancel_button_) {
      self->resolve_dialog(false);
    } else if (target == self->dialog_confirm_button_) {
      self->resolve_dialog(true);
    }
  }
}

void UiBasicsPage::record_press() noexcept {
  if (state_.press_count < std::numeric_limits<std::uint16_t>::max()) {
    ++state_.press_count;
  }
  state_.last_action = PageAction::kPress;
  update_status();
}

void UiBasicsPage::record_long_press() noexcept {
  if (state_.long_press_count < std::numeric_limits<std::uint16_t>::max()) {
    ++state_.long_press_count;
  }
  state_.last_action = PageAction::kLongPress;
  update_status();
}

bool UiBasicsPage::show_toast() noexcept {
  clear_overlay();
  overlay_ = lv_obj_create(root_);
  if (overlay_ == nullptr) {
    return false;
  }
  context_.theme->apply_toast(overlay_);
  lv_obj_add_flag(overlay_, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(overlay_, 520, 64);
  lv_obj_align(overlay_, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *label = create_fixed_label(
      overlay_, context_.locale, generated::TextId::ui_basics_toast_success,
      *context_.theme, hmi::ui::TextRole::kInverse);
  if (label == nullptr) {
    clear_overlay();
    return false;
  }
  lv_obj_center(label);
  toast_timer_ = lv_timer_create(&UiBasicsPage::handle_toast_timer, 1200, this);
  if (toast_timer_ == nullptr) {
    clear_overlay();
    return false;
  }
  lv_timer_set_repeat_count(toast_timer_, 1);
  state_.overlay = OverlayState::kToast;
  state_.last_action = PageAction::kToastShown;
  update_status();
  return true;
}

void UiBasicsPage::handle_toast_timer(lv_timer_t *timer) {
  auto *self =
      static_cast<UiBasicsPage *>(timer == nullptr ? nullptr : timer->user_data);
  if (self != nullptr) {
    self->toast_timer_ = nullptr;
    self->expire_toast();
  }
}

void UiBasicsPage::expire_toast() noexcept {
  if (toast_timer_ != nullptr) {
    lv_timer_del(toast_timer_);
    toast_timer_ = nullptr;
  }
  if (state_.overlay == OverlayState::kToast && overlay_ != nullptr) {
    lv_obj_del(overlay_);
    overlay_ = nullptr;
    state_.overlay = OverlayState::kNone;
    state_.last_action = PageAction::kToastExpired;
    update_status();
  }
}

bool UiBasicsPage::open_dialog() noexcept {
  clear_overlay();
  lv_obj_t *content = root_ == nullptr ? nullptr : lv_obj_get_parent(root_);
  if (content == nullptr) {
    return false;
  }
  // dim layer는 page root의 18px padding 영향을 받지 않도록 화면
  // content의 직계 자식으로 둔다.
  overlay_ = lv_obj_create(content);
  if (overlay_ == nullptr) {
    return false;
  }
  context_.theme->apply_scrim(overlay_);
  lv_obj_add_flag(overlay_, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(overlay_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(overlay_, 0, 0);
  lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *dialog = lv_obj_create(overlay_);
  if (dialog == nullptr) {
    clear_overlay();
    return false;
  }
  context_.theme->apply_dialog(dialog);
  lv_obj_set_size(dialog, 560, 260);
  lv_obj_center(dialog);
  lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(dialog, 26, 0);
  lv_obj_set_style_pad_row(dialog, 16, 0);
  lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(dialog, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title = create_fixed_label(
      dialog, context_.locale, generated::TextId::ui_basics_dialog_title,
      *context_.theme, hmi::ui::TextRole::kTitle);
  lv_obj_t *body = create_fixed_label(
      dialog, context_.locale, generated::TextId::ui_basics_dialog_body,
      *context_.theme, hmi::ui::TextRole::kMuted);
  if (title == nullptr || body == nullptr) {
    clear_overlay();
    return false;
  }
  hmi::ui::configure_multiline(body, 480, 62, 6, LV_TEXT_ALIGN_CENTER);

  lv_obj_t *buttons = lv_obj_create(dialog);
  if (buttons == nullptr) {
    clear_overlay();
    return false;
  }
  configure_row(buttons, *context_.theme);
  lv_obj_set_width(buttons, 450);
  lv_obj_set_style_pad_column(buttons, 14, 0);
  dialog_cancel_button_ = create_fixed_button(
      buttons, context_.locale, generated::TextId::ui_basics_dialog_cancel,
      *context_.theme, hmi::ui::ButtonVariant::kSecondary,
      &UiBasicsPage::handle_event, this);
  dialog_confirm_button_ = create_fixed_button(
      buttons, context_.locale, generated::TextId::ui_basics_dialog_confirm,
      *context_.theme, hmi::ui::ButtonVariant::kPrimary,
      &UiBasicsPage::handle_event, this);
  if (dialog_cancel_button_ == nullptr || dialog_confirm_button_ == nullptr) {
    clear_overlay();
    return false;
  }
  lv_obj_set_size(dialog_cancel_button_, 210, 52);
  lv_obj_set_size(dialog_confirm_button_, 210, 52);
  state_.overlay = OverlayState::kConfirm;
  state_.last_action = PageAction::kDialogOpened;
  update_status();
  return true;
}

void UiBasicsPage::resolve_dialog(bool confirmed) noexcept {
  if (state_.overlay != OverlayState::kConfirm) {
    return;
  }
  if (overlay_ != nullptr) {
    lv_obj_del(overlay_);
  }
  overlay_ = nullptr;
  dialog_cancel_button_ = nullptr;
  dialog_confirm_button_ = nullptr;
  state_.overlay = OverlayState::kNone;
  state_.last_action = confirmed ? PageAction::kDialogConfirmed
                                 : PageAction::kDialogCancelled;
  update_status();
}

void UiBasicsPage::update_status() noexcept {
  if (status_label_ == nullptr) {
    return;
  }
  generated::TextId action_id = generated::TextId::ui_basics_status_ready;
  switch (state_.last_action) {
    case PageAction::kPress:
      action_id = generated::TextId::ui_basics_status_short_press;
      break;
    case PageAction::kLongPress:
      action_id = generated::TextId::ui_basics_status_long_press;
      break;
    case PageAction::kToastShown:
      action_id = generated::TextId::ui_basics_status_toast_shown;
      break;
    case PageAction::kToastExpired:
      action_id = generated::TextId::ui_basics_status_toast_expired;
      break;
    case PageAction::kDialogOpened:
      action_id = generated::TextId::ui_basics_status_dialog_opened;
      break;
    case PageAction::kDialogCancelled:
      action_id = generated::TextId::ui_basics_status_dialog_cancelled;
      break;
    case PageAction::kDialogConfirmed:
      action_id = generated::TextId::ui_basics_status_dialog_confirmed;
      break;
    case PageAction::kNone: break;
  }
  const std::array<hmi::i18n::FormatArgument, 3U> arguments = {
      hmi::i18n::FormatArgument::text(
          "action", generated::text(context_.locale, action_id)),
      hmi::i18n::FormatArgument::number("press", state_.press_count),
      hmi::i18n::FormatArgument::number("long", state_.long_press_count),
  };
  std::array<char, 192U> buffer{};
  const auto result = hmi::i18n::format_to(
      generated::text(context_.locale,
                      generated::TextId::ui_basics_status_summary),
      arguments.data(), arguments.size(), buffer.data(), buffer.size());
  if (result.status == hmi::i18n::FormatStatus::ok) {
    lv_label_set_text(status_label_, buffer.data());
  }
}

void UiBasicsPage::clear_overlay() noexcept {
  if (toast_timer_ != nullptr) {
    lv_timer_del(toast_timer_);
    toast_timer_ = nullptr;
  }
  if (overlay_ != nullptr) {
    lv_obj_del(overlay_);
  }
  overlay_ = nullptr;
  dialog_cancel_button_ = nullptr;
  dialog_confirm_button_ = nullptr;
  state_.overlay = OverlayState::kNone;
}

PageSnapshot UiBasicsPage::snapshot() const noexcept { return state_; }

void UiBasicsPage::destroy() noexcept {
  clear_overlay();
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  status_label_ = nullptr;
  back_button_ = nullptr;
  locale_ko_button_ = nullptr;
  locale_en_button_ = nullptr;
  theme_button_ = nullptr;
  press_button_ = nullptr;
  hold_button_ = nullptr;
  toast_button_ = nullptr;
  dialog_button_ = nullptr;
  context_ = {};
}

UiBasicsPage::~UiBasicsPage() { destroy(); }

}  // namespace hmi::samples::suite
