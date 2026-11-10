#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <lvgl.h>

#include "dashboard/dashboard_page.hpp"
#include "hmi/i18n/locale.hpp"
#include "hmi/ui/theme.hpp"
#include "page_contract.hpp"
#include "profiles/runtime/app_contract.hpp"
#include "storage_access_lock.hpp"

namespace hmi::samples::suite {

enum class SuiteView : std::uint8_t { kNotCreated, kDashboard, kPage, kError };

struct SampleSuiteSnapshot {
  SuiteView view = SuiteView::kNotCreated;
  hmi::ui::ThemeMode theme_mode = hmi::ui::ThemeMode::kLight;
  hmi::i18n::Locale locale = hmi::i18n::Locale::ko_kr;
  const char *active_page_id = nullptr;
  PageSnapshot page{};
  StorageAccessLockSnapshot storage_lock{};
};

/**
 * Dashboard와 현재 선택 page 하나의 수명을 조립하는 sample profile 앱이다.
 *
 * LVGL callback은 전환 요청만 남기고 lv_async_call의 안전한 시점에 화면을 바꾼다.
 * 앱은 screen/content, Dashboard, active page를 소유하며 destroy에서 예약된 전환을
 * 취소한 뒤 page의 timer와 widget이 먼저 정리되도록 한다.
 */
class SampleSuiteApp {
 public:
  SampleSuiteApp() = default;
  SampleSuiteApp(const SampleSuiteApp &) = delete;
  SampleSuiteApp &operator=(const SampleSuiteApp &) = delete;

  bool create(const hmi::profiles::AppContext &context);
  void destroy() noexcept;
  ~SampleSuiteApp();

  bool request_page(const char *page_id) noexcept;
  bool request_dashboard() noexcept;
  bool request_theme_toggle() noexcept;
  bool handle_utf8_input(const char *utf8) noexcept;
  bool handle_key(hmi::profiles::AppKey key) noexcept;
  bool flush_pending_transition() noexcept;
  void refresh_storage_access_lock() noexcept;
  [[nodiscard]] SampleSuiteSnapshot snapshot() const noexcept;

 private:
  enum class PendingView : std::uint8_t {
    kNone,
    kDashboard,
    kPage,
    kLocaleRefresh,
    kThemeToggle,
  };

  static void handle_dashboard_selection(void *user_data,
                                         const char *page_id) noexcept;
  static void handle_page_back(void *user_data) noexcept;
  static void handle_theme_toggle(void *user_data) noexcept;
  static void handle_storage_lock_refresh(void *user_data) noexcept;
  static void handle_storage_timer(lv_timer_t *timer) noexcept;
  static bool handle_locale_change(void *user_data,
                                   hmi::i18n::Locale locale) noexcept;
  static void process_transition_async(void *user_data);
  bool schedule_transition() noexcept;
  bool apply_pending_transition() noexcept;
  bool show_dashboard() noexcept;
  bool show_page(std::size_t descriptor_index) noexcept;
  [[nodiscard]] PageContext make_page_context() noexcept;
  void destroy_active_view() noexcept;
  bool rebuild_active_page() noexcept;
  bool rebuild_for_theme_toggle() noexcept;
  bool rebuild_storage_lock() noexcept;
  [[nodiscard]] bool storage_locked() const noexcept;

  lv_obj_t *screen_ = nullptr;
  lv_obj_t *content_ = nullptr;
  hmi::ui::UiTheme theme_{};
  hmi::profiles::BinaryFontPort fonts_{};
  hmi::settings::LevelSettingsPort level_settings_{};
  hmi::audio::AudioPlaybackPort* audio_playback_ = nullptr;
  hmi::connectivity::WifiApWebController* ap_control_ = nullptr;
  hmi::connectivity::WifiStaRegistration* wifi_registration_ = nullptr;
  hmi::storage::StorageAccessController* storage_access_ = nullptr;
  StorageAccessLock storage_lock_{};
  lv_timer_t* storage_timer_ = nullptr;
  hmi::i18n::Locale locale_ = hmi::i18n::Locale::ko_kr;
  DashboardPage dashboard_{};
  std::unique_ptr<SamplePage> active_page_{};
  const PageDescriptor *active_descriptor_ = nullptr;
  PendingView pending_view_ = PendingView::kNone;
  std::size_t pending_page_index_ = 0;
  std::size_t active_page_index_ = 0;
  bool transition_scheduled_ = false;
  bool storage_audio_stopped_ = false;
  SuiteView view_ = SuiteView::kNotCreated;
};

}  // namespace hmi::samples::suite
