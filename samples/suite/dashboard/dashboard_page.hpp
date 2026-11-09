#pragma once

#include <cstddef>

#include <lvgl.h>

#include "hmi/i18n/locale.hpp"
#include "hmi/ui/theme.hpp"
#include "page_contract.hpp"

namespace hmi::samples::suite {

/**
 * 생성된 descriptor 목록만 표시하는 sample-suite 첫 화면이다.
 *
 * 개별 page 타입과 생성 규칙은 알지 못한다. root widget을 소유하며 선택 callback은
 * 화면 전환을 직접 수행하지 않고 SampleSuiteApp에 안전한 전환 요청만 전달한다.
 */
class DashboardPage {
 public:
  using SelectCallback = void (*)(void *user_data, const char *page_id) noexcept;
  using ThemeCallback = void (*)(void *user_data) noexcept;
  using LocaleCallback = bool (*)(void *user_data,
                                  hmi::i18n::Locale locale) noexcept;

  DashboardPage() = default;
  DashboardPage(const DashboardPage &) = delete;
  DashboardPage &operator=(const DashboardPage &) = delete;

  bool create(lv_obj_t *parent, const PageDescriptor *descriptors,
              std::size_t descriptor_count, SelectCallback on_select,
              ThemeCallback on_theme_toggle, LocaleCallback on_locale_change,
              hmi::i18n::Locale locale, hmi::ui::UiTheme &theme, void *user_data);
  void destroy() noexcept;
  ~DashboardPage();

 private:
  static void handle_page_event(lv_event_t *event);
  static void handle_theme_event(lv_event_t *event);
  static void handle_locale_event(lv_event_t *event);

  lv_obj_t *root_ = nullptr;
  SelectCallback on_select_ = nullptr;
  ThemeCallback on_theme_toggle_ = nullptr;
  LocaleCallback on_locale_change_ = nullptr;
  lv_obj_t *locale_ko_button_ = nullptr;
  lv_obj_t *locale_en_button_ = nullptr;
  void *user_data_ = nullptr;
};

}  // namespace hmi::samples::suite
