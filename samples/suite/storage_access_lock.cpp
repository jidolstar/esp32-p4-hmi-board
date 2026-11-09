#include "storage_access_lock.hpp"

#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {
namespace {

using TextId = generated::TextId;
using State = hmi::storage::StorageAccessState;

void set_fixed_text(lv_obj_t* label, hmi::i18n::Locale locale,
                    TextId id) noexcept {
  if (label == nullptr) {
    return;
  }
  lv_label_set_text(label, generated::text(locale, id).data());
  lv_obj_set_style_text_font(
      label, generated::fixed_font(generated::typography(id)), 0);
}

TextId title_id(State state, bool confirming) noexcept {
  if (confirming) {
    return TextId::storage_lock_confirm_title;
  }
  switch (state) {
    case State::quiescing:
      return TextId::storage_lock_quiescing_title;
    case State::exposing:
      return TextId::storage_lock_exposing_title;
    case State::waiting_for_host:
      return TextId::storage_lock_waiting_title;
    case State::pc_active:
      return TextId::storage_lock_active_title;
    case State::reclaiming:
      return TextId::storage_lock_reclaiming_title;
    case State::remount_failed:
      return TextId::storage_lock_failed_title;
    case State::unavailable:
    case State::hmi_ready:
      return TextId::storage_lock_quiescing_title;
  }
  return TextId::storage_lock_failed_title;
}

TextId body_id(State state, bool confirming) noexcept {
  if (confirming) {
    return TextId::storage_lock_confirm_body;
  }
  switch (state) {
    case State::quiescing:
      return TextId::storage_lock_quiescing_body;
    case State::exposing:
      return TextId::storage_lock_exposing_body;
    case State::waiting_for_host:
      return TextId::storage_lock_waiting_body;
    case State::pc_active:
      return TextId::storage_lock_active_body;
    case State::reclaiming:
      return TextId::storage_lock_reclaiming_body;
    case State::remount_failed:
      return TextId::storage_lock_failed_body;
    case State::unavailable:
    case State::hmi_ready:
      return TextId::storage_lock_quiescing_body;
  }
  return TextId::storage_lock_failed_body;
}

TextId status_id(const hmi::storage::StorageAccessSnapshot& snapshot) noexcept {
  if (snapshot.error == hmi::storage::StorageAccessError::callback_drain_timeout) {
    return TextId::storage_lock_error_drain;
  }
  if (snapshot.state == State::pc_active && snapshot.suspended) {
    return TextId::storage_lock_status_suspended;
  }
  switch (snapshot.state) {
    case State::waiting_for_host:
      return TextId::storage_lock_status_waiting;
    case State::pc_active:
      return TextId::storage_lock_status_active;
    case State::remount_failed:
      return TextId::storage_lock_status_failed;
    case State::quiescing:
    case State::exposing:
    case State::reclaiming:
    case State::unavailable:
    case State::hmi_ready:
      return TextId::storage_lock_status_working;
  }
  return TextId::storage_lock_status_failed;
}

}  // namespace

bool StorageAccessLock::create(
    lv_obj_t* parent, hmi::storage::StorageAccessController& controller,
    hmi::i18n::Locale locale, hmi::ui::UiTheme& theme) noexcept {
  if (parent == nullptr || root_ != nullptr || !theme.initialized()) {
    return false;
  }
  controller_ = &controller;
  theme_ = &theme;
  locale_ = locale;

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    destroy();
    return false;
  }
  theme.apply_screen(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  card_ = lv_obj_create(root_);
  if (card_ == nullptr) {
    destroy();
    return false;
  }
  theme.apply_card(card_);
  lv_obj_set_size(card_, 680, 390);
  lv_obj_clear_flag(card_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(card_, 32, 0);
  lv_obj_set_style_pad_row(card_, 18, 0);
  lv_obj_set_flex_flow(card_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  title_ = hmi::ui::create_label(card_, "", theme, hmi::ui::TextRole::kDisplay);
  body_ = hmi::ui::create_label(card_, "", theme, hmi::ui::TextRole::kBody);
  status_ = hmi::ui::create_label(card_, "", theme, hmi::ui::TextRole::kAccent);
  action_ = hmi::ui::create_button(
      card_, "", theme, hmi::ui::ButtonVariant::kPrimary,
      &StorageAccessLock::handle_action, this);
  if (title_ == nullptr || body_ == nullptr || status_ == nullptr ||
      action_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_width(title_, 600);
  lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
  hmi::ui::configure_multiline(body_, 590, 112, 6);
  lv_obj_set_style_text_align(body_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(status_, 590);
  lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_size(action_, 260, 56);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  return true;
}

void StorageAccessLock::refresh() noexcept {
  if (controller_ == nullptr || root_ == nullptr) {
    return;
  }
  const auto value = controller_->snapshot();
  if (!value.lock_required) {
    return_confirmation_ = false;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(root_);
  update_view(value);
  lv_obj_invalidate(root_);
}

void StorageAccessLock::update_view(
    const hmi::storage::StorageAccessSnapshot& value) noexcept {
  visible_state_ = value.state;
  visible_error_ = value.error;
  if (value.state != State::waiting_for_host && value.state != State::pc_active) {
    return_confirmation_ = false;
  }
  set_fixed_text(title_, locale_, title_id(value.state, return_confirmation_));
  set_fixed_text(body_, locale_, body_id(value.state, return_confirmation_));
  set_fixed_text(status_, locale_, status_id(value));

  const bool can_return = value.state == State::waiting_for_host ||
                          value.state == State::pc_active;
  const bool can_retry = value.state == State::remount_failed;
  if (can_return || can_retry) {
    const TextId action_id = can_retry
                                 ? TextId::storage_action_retry
                                 : (return_confirmation_
                                        ? TextId::storage_action_confirm_return
                                        : TextId::storage_action_return);
    lv_obj_clear_flag(action_, LV_OBJ_FLAG_HIDDEN);
    set_fixed_text(lv_obj_get_child(action_, 0), locale_, action_id);
  } else {
    lv_obj_add_flag(action_, LV_OBJ_FLAG_HIDDEN);
  }
}

void StorageAccessLock::handle_action(lv_event_t* event) {
  if (event == nullptr || lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  auto* self = static_cast<StorageAccessLock*>(lv_event_get_user_data(event));
  if (self != nullptr) {
    self->request_action();
  }
}

bool StorageAccessLock::request_action() noexcept {
  if (controller_ == nullptr) {
    return false;
  }
  const auto value = controller_->snapshot();
  if (value.state == State::remount_failed) {
    const bool accepted = controller_->retry_remount(lv_tick_get());
    refresh();
    return accepted;
  }
  if (value.state != State::waiting_for_host && value.state != State::pc_active) {
    return false;
  }
  if (!return_confirmation_) {
    return_confirmation_ = true;
    update_view(value);
    return true;
  }
  const bool accepted = controller_->request_hmi_access(
      lv_tick_get(), hmi::storage::StorageReclaimReason::explicit_request);
  if (accepted) {
    return_confirmation_ = false;
  }
  refresh();
  return accepted;
}

StorageAccessLockSnapshot StorageAccessLock::snapshot() const noexcept {
  return {visible_state_, visible_error_,
          root_ != nullptr && !lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN),
          return_confirmation_};
}

void StorageAccessLock::destroy() noexcept {
  if (root_ != nullptr) {
    lv_obj_del(root_);
  }
  root_ = nullptr;
  card_ = nullptr;
  title_ = nullptr;
  body_ = nullptr;
  status_ = nullptr;
  action_ = nullptr;
  controller_ = nullptr;
  theme_ = nullptr;
  return_confirmation_ = false;
  visible_state_ = State::unavailable;
  visible_error_ = hmi::storage::StorageAccessError::none;
}

StorageAccessLock::~StorageAccessLock() { destroy(); }

}  // namespace hmi::samples::suite
