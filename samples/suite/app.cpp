#include "app.hpp"

#include <cstring>
#include <new>

#include "page_registry.hpp"

namespace hmi::samples::suite {

static_assert(sizeof(SampleSuiteApp) <= 2304U,
              "SampleSuiteApp must remain a small one-page composition root");

bool SampleSuiteApp::create(const hmi::profiles::AppContext &context) {
  if (screen_ != nullptr || context.host_name == nullptr ||
      context.host_name[0] == '\0') {
    return false;
  }
  if (!theme_.initialize(hmi::ui::ThemeMode::kLight)) {
    return false;
  }
  fonts_ = context.fonts;
  level_settings_ = context.level_settings;
  wifi_registration_ = context.wifi_registration;
  screen_ = lv_obj_create(nullptr);
  if (screen_ == nullptr) {
    theme_.reset();
    return false;
  }
  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
  theme_.apply_screen(screen_);

  content_ = lv_obj_create(screen_);
  if (content_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(content_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(content_, 0, 0);
  lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
  theme_.apply_screen(content_);

  if (!show_dashboard()) {
    destroy();
    return false;
  }
  lv_scr_load(screen_);
  return true;
}

bool SampleSuiteApp::request_page(const char *page_id) noexcept {
  if (screen_ == nullptr || page_id == nullptr) {
    return false;
  }
  const PageDescriptor *descriptors = generated::page_descriptors();
  const std::size_t count = generated::page_descriptor_count();
  for (std::size_t index = 0; index < count; ++index) {
    if (std::strcmp(descriptors[index].id, page_id) == 0) {
      pending_view_ = PendingView::kPage;
      pending_page_index_ = index;
      return schedule_transition();
    }
  }
  return false;
}

bool SampleSuiteApp::request_dashboard() noexcept {
  if (screen_ == nullptr) {
    return false;
  }
  pending_view_ = PendingView::kDashboard;
  return schedule_transition();
}

bool SampleSuiteApp::request_theme_toggle() noexcept {
  if (screen_ == nullptr) {
    return false;
  }
  pending_view_ = PendingView::kThemeToggle;
  return schedule_transition();
}

bool SampleSuiteApp::schedule_transition() noexcept {
  if (transition_scheduled_) {
    return true;
  }
  if (lv_async_call(&SampleSuiteApp::process_transition_async, this) != LV_RES_OK) {
    pending_view_ = PendingView::kNone;
    return false;
  }
  transition_scheduled_ = true;
  return true;
}

void SampleSuiteApp::process_transition_async(void *user_data) {
  auto *self = static_cast<SampleSuiteApp *>(user_data);
  if (self != nullptr) {
    self->transition_scheduled_ = false;
    self->apply_pending_transition();
  }
}

bool SampleSuiteApp::flush_pending_transition() noexcept {
  if (transition_scheduled_) {
    lv_async_call_cancel(&SampleSuiteApp::process_transition_async, this);
    transition_scheduled_ = false;
  }
  return apply_pending_transition();
}

bool SampleSuiteApp::apply_pending_transition() noexcept {
  const PendingView target = pending_view_;
  const std::size_t page_index = pending_page_index_;
  pending_view_ = PendingView::kNone;
  if (target == PendingView::kNone) {
    return true;
  }
  if (target == PendingView::kDashboard) {
    return show_dashboard();
  }
  if (target == PendingView::kLocaleRefresh) {
    return rebuild_active_page();
  }
  if (target == PendingView::kThemeToggle) {
    return rebuild_for_theme_toggle();
  }
  return show_page(page_index);
}

void SampleSuiteApp::destroy_active_view() noexcept {
  active_page_.reset();
  active_descriptor_ = nullptr;
  dashboard_.destroy();
}

bool SampleSuiteApp::rebuild_active_page() noexcept {
  if (view_ != SuiteView::kPage || active_page_ == nullptr) {
    return false;
  }
  // page controller는 유지하고 widget만 다시 만들어 locale 전환 중 상태를 보존한다.
  active_page_->destroy();
  if (!active_page_->create(content_, make_page_context())) {
    active_page_.reset();
    active_descriptor_ = nullptr;
    view_ = SuiteView::kError;
    return false;
  }
  lv_obj_invalidate(screen_);
  return true;
}

bool SampleSuiteApp::rebuild_for_theme_toggle() noexcept {
  const SuiteView previous_view = view_;
  const std::size_t previous_page_index = active_page_index_;
  if (previous_view == SuiteView::kPage && active_page_ != nullptr) {
    // page 객체는 유지하고 widget/font만 해제해 in-memory 입력 상태를 보존한다.
    active_page_->destroy();
    dashboard_.destroy();
  } else {
    destroy_active_view();
  }

  // screen/content도 테마 스타일을 참조하므로 재설정 전에 연결을 먼저 끊는다.
  lv_obj_remove_style_all(content_);
  lv_obj_remove_style_all(screen_);
  if (!theme_.set_mode(theme_.opposite_mode())) {
    view_ = SuiteView::kError;
    return false;
  }
  theme_.apply_screen(screen_);
  theme_.apply_screen(content_);
  lv_obj_set_size(content_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(content_, 0, 0);
  lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);

  if (previous_view == SuiteView::kPage) {
    if (active_page_ == nullptr ||
        !active_page_->create(content_, make_page_context())) {
      active_page_.reset();
      active_descriptor_ = nullptr;
      view_ = SuiteView::kError;
      return false;
    }
    active_page_index_ = previous_page_index;
    view_ = SuiteView::kPage;
    return true;
  }
  return show_dashboard();
}

bool SampleSuiteApp::show_dashboard() noexcept {
  destroy_active_view();
  // callback 안에서 선택된 control이 삭제되어도 공통 content의
  // scroll offset과 이전 invalidation이 새 Dashboard 배치에 남지 않게 한다.
  lv_obj_scroll_to(content_, 0, 0, LV_ANIM_OFF);
  if (!dashboard_.create(content_, generated::page_descriptors(),
                         generated::page_descriptor_count(),
                         &SampleSuiteApp::handle_dashboard_selection,
                         &SampleSuiteApp::handle_theme_toggle,
                         &SampleSuiteApp::handle_locale_change, locale_, theme_,
                         this)) {
    view_ = SuiteView::kError;
    return false;
  }
  lv_obj_invalidate(screen_);
  view_ = SuiteView::kDashboard;
  return true;
}

bool SampleSuiteApp::show_page(std::size_t descriptor_index) noexcept {
  const std::size_t count = generated::page_descriptor_count();
  if (descriptor_index >= count) {
    view_ = SuiteView::kError;
    return false;
  }
  const PageDescriptor &descriptor = generated::page_descriptors()[descriptor_index];
  std::unique_ptr<SamplePage> page(descriptor.create());
  if (!page) {
    return false;
  }
  PageContext page_context = make_page_context();
  if (!page->create(content_, page_context)) {
    page->destroy();
    return false;
  }
  dashboard_.destroy();
  active_page_ = std::move(page);
  active_descriptor_ = &descriptor;
  active_page_index_ = descriptor_index;
  view_ = SuiteView::kPage;
  return true;
}

PageContext SampleSuiteApp::make_page_context() noexcept {
  PageContext page_context;
  page_context.request_dashboard = &SampleSuiteApp::handle_page_back;
  page_context.request_theme_toggle = &SampleSuiteApp::handle_theme_toggle;
  page_context.set_locale = &SampleSuiteApp::handle_locale_change;
  page_context.locale = locale_;
  page_context.fonts = fonts_;
  page_context.level_settings = &level_settings_;
  page_context.wifi_registration = wifi_registration_;
  page_context.theme = &theme_;
  page_context.user_data = this;
  return page_context;
}

void SampleSuiteApp::handle_dashboard_selection(void *user_data,
                                                const char *page_id) noexcept {
  auto *self = static_cast<SampleSuiteApp *>(user_data);
  if (self != nullptr) {
    self->request_page(page_id);
  }
}

void SampleSuiteApp::handle_page_back(void *user_data) noexcept {
  auto *self = static_cast<SampleSuiteApp *>(user_data);
  if (self != nullptr) {
    self->request_dashboard();
  }
}

void SampleSuiteApp::handle_theme_toggle(void *user_data) noexcept {
  auto *self = static_cast<SampleSuiteApp *>(user_data);
  if (self != nullptr) {
    self->request_theme_toggle();
  }
}

bool SampleSuiteApp::handle_locale_change(void *user_data,
                                          hmi::i18n::Locale locale) noexcept {
  auto *self = static_cast<SampleSuiteApp *>(user_data);
  if (self == nullptr) {
    return false;
  }
  if (self->locale_ == locale) {
    return true;
  }
  self->locale_ = locale;
  if (self->view_ == SuiteView::kDashboard) {
    self->pending_view_ = PendingView::kDashboard;
    return self->schedule_transition();
  }
  if (self->view_ == SuiteView::kPage) {
    self->pending_view_ = PendingView::kLocaleRefresh;
    return self->schedule_transition();
  }
  return true;
}

bool SampleSuiteApp::handle_utf8_input(const char *utf8) noexcept {
  return active_page_ != nullptr && active_page_->handle_utf8_input(utf8);
}

bool SampleSuiteApp::handle_key(hmi::profiles::AppKey key) noexcept {
  return active_page_ != nullptr && active_page_->handle_key(key);
}

SampleSuiteSnapshot SampleSuiteApp::snapshot() const noexcept {
  SampleSuiteSnapshot value;
  value.view = view_;
  value.theme_mode = theme_.mode();
  value.locale = locale_;
  value.active_page_id =
      active_descriptor_ == nullptr ? nullptr : active_descriptor_->id;
  if (active_page_ != nullptr) {
    value.page = active_page_->snapshot();
  }
  return value;
}

void SampleSuiteApp::destroy() noexcept {
  if (transition_scheduled_) {
    lv_async_call_cancel(&SampleSuiteApp::process_transition_async, this);
  }
  transition_scheduled_ = false;
  pending_view_ = PendingView::kNone;
  destroy_active_view();
  if (screen_ != nullptr && lv_scr_act() == screen_) {
    lv_obj_t *empty = lv_obj_create(nullptr);
    if (empty != nullptr) {
      lv_scr_load(empty);
    }
  }
  if (screen_ != nullptr) {
    lv_obj_del(screen_);
  }
  screen_ = nullptr;
  content_ = nullptr;
  theme_.reset();
  view_ = SuiteView::kNotCreated;
  fonts_ = {};
  level_settings_ = {};
  wifi_registration_ = nullptr;
}

SampleSuiteApp::~SampleSuiteApp() { destroy(); }

}  // namespace hmi::samples::suite
