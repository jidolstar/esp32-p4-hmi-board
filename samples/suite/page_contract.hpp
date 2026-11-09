#pragma once

#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "hmi/i18n/locale.hpp"
#include "hmi/ui/theme.hpp"
#include "profiles/runtime/app_contract.hpp"

namespace hmi::samples::suite {

enum class OverlayState : std::uint8_t { kNone, kToast, kConfirm };
enum class PageAction : std::uint8_t {
  kNone,
  kPress,
  kLongPress,
  kToastShown,
  kToastExpired,
  kDialogOpened,
  kDialogCancelled,
  kDialogConfirmed,
};

enum class SettingsPageStatus : std::uint8_t {
  kUnavailable,
  kReady,
  kSaved,
  kApplyFailed,
  kSaveFailed,
  kPreviewUnavailable,
};

struct PageSnapshot {
  OverlayState overlay = OverlayState::kNone;
  PageAction last_action = PageAction::kNone;
  std::uint16_t press_count = 0;
  std::uint16_t long_press_count = 0;
  std::uint8_t brightness = 0;
  std::uint8_t volume = 0;
  SettingsPageStatus settings_status = SettingsPageStatus::kUnavailable;
};

struct PageContext {
  void (*request_dashboard)(void *user_data) noexcept = nullptr;
  void (*request_theme_toggle)(void *user_data) noexcept = nullptr;
  bool (*set_locale)(void *user_data, hmi::i18n::Locale locale) noexcept = nullptr;
  hmi::i18n::Locale locale = hmi::i18n::Locale::ko_kr;
  hmi::profiles::BinaryFontPort fonts{};
  const hmi::settings::LevelSettingsPort* level_settings = nullptr;
  hmi::connectivity::WifiStaRegistration* wifi_registration = nullptr;
  hmi::ui::UiTheme *theme = nullptr;
  void *user_data = nullptr;
};

/**
 * Dashboard가 수명만 관리하는 sample page의 최소 계약이다.
 *
 * 구현은 create에서 parent 아래 widget을 만들고 destroy에서 timer, overlay와
 * widget을 모두 정리한다. 동시에 한 구현만 살아 있으며 raw widget은 계약 밖으로
 * 노출하지 않는다.
 */
class SamplePage {
 public:
  SamplePage() = default;
  SamplePage(const SamplePage &) = delete;
  SamplePage &operator=(const SamplePage &) = delete;
  virtual ~SamplePage() = default;

  virtual bool create(lv_obj_t *parent, const PageContext &context) = 0;
  virtual void destroy() noexcept = 0;
  [[nodiscard]] virtual PageSnapshot snapshot() const noexcept = 0;
  virtual bool handle_utf8_input(const char *) noexcept { return false; }
  virtual bool handle_key(hmi::profiles::AppKey) noexcept { return false; }
};

using PageFactory = SamplePage *(*)() noexcept;

struct PageDescriptor {
  const char *id;
  const char *title_key;
  const char *description_key;
  std::uint16_t order;
  const char *data_key;
  PageFactory create;
};

}  // namespace hmi::samples::suite
