#include <array>
#include <cassert>
#include <cstdio>
#include <memory>
#include <string_view>

#include <lvgl.h>
#include <src/extra/libs/fsdrv/lv_fsdrv.h>

#include "connectivity/virtual_wifi_sta.hpp"
#include "hmi/connectivity/wifi_station_settings.hpp"
#include "pages/wifi/wifi_settings_page.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

void ignore(void*) noexcept {}

bool accept_locale(void*, hmi::i18n::Locale) noexcept { return true; }

void* load_font(void*, const char* relative_path, std::uint32_t size_bytes,
                std::uint32_t crc32) noexcept {
  std::array<char, 320U> path{};
  const int written = std::snprintf(path.data(), path.size(),
                                    "A:samples/suite/%s", relative_path);
  if (written <= 0 || static_cast<std::size_t>(written) >= path.size()) {
    return nullptr;
  }
  return hmi::ui::load_binary_font_file(path.data(), size_bytes, crc32);
}

void free_font(void*, void* font) noexcept {
  lv_font_free(static_cast<lv_font_t*>(font));
}

void advance(hmi::connectivity::WifiStaRegistration& registration,
             hmi::samples::suite::WifiSettingsPage& page,
             std::uint32_t& now_ms, unsigned count) {
  for (unsigned index = 0U; index < count; ++index) {
    now_ms += 100U;
    registration.tick(now_ms);
    assert(page.refresh());
  }
}

}  // namespace

int main() {
  using hmi::connectivity::WifiRegistrationState;
  using hmi::samples::suite::WifiInputField;
  using hmi::samples::suite::WifiSettingsPage;

  lv_init();
  lv_fs_stdio_init();
  auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
  assert(port->begin("wifi settings state", false));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  hmi::platform::connectivity::VirtualWifiSta wifi;
  hmi::platform::connectivity::MemoryWifiStationSettingsStore store;
  hmi::connectivity::WifiStaRegistration registration(wifi, store);
  std::uint32_t now_ms = 0U;
  assert(registration.begin(now_ms, true));

  hmi::ui::UiTheme theme;
  assert(theme.initialize());
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &ignore;
  context.request_theme_toggle = &ignore;
  context.set_locale = &accept_locale;
  context.locale = hmi::i18n::Locale::ko_kr;
  context.fonts = {nullptr, &load_font, &free_font};
  context.wifi_registration = &registration;
  context.theme = &theme;

  // 202609 화면의 지연 scan 계약: 저장 자격 증명 자동 연결 중에 화면을
  // 열거나 새로고침해도 요청을 잃지 않고 연결 종료 직후 목록을 채운다.
  {
    hmi::platform::connectivity::VirtualWifiSta restore_wifi;
    restore_wifi.set_scenario(
        hmi::platform::connectivity::VirtualWifiScenario::authentication_failure);
    hmi::platform::connectivity::MemoryWifiStationSettingsStore restore_store;
    hmi::connectivity::WifiStationCredential saved{};
    constexpr std::string_view saved_ssid = "HMI-LAB";
    constexpr std::string_view saved_password = "starter123";
    assert(hmi::connectivity::make_wifi_station_credential(
               reinterpret_cast<const std::uint8_t*>(saved_ssid.data()),
               saved_ssid.size(),
               reinterpret_cast<const std::uint8_t*>(saved_password.data()),
               saved_password.size(),
               hmi::connectivity::CredentialInputSource::service_tool,
               saved) == hmi::connectivity::CredentialValidationError::none);
    assert(restore_store.save(saved));
    hmi::connectivity::clear_wifi_station_credential(saved);

    hmi::connectivity::WifiStaRegistration restore_registration(
        restore_wifi, restore_store);
    std::uint32_t restore_now_ms = 0U;
    assert(restore_registration.begin(restore_now_ms, true));
    auto restore_context = context;
    restore_context.wifi_registration = &restore_registration;
    WifiSettingsPage restore_page;
    assert(restore_page.create(screen, restore_context));
    assert(restore_page.wifi_snapshot().scan_pending);
    assert(restore_page.request_scan());
    advance(restore_registration, restore_page, restore_now_ms, 9U);
    assert(!restore_page.wifi_snapshot().scan_pending);
    assert(restore_page.wifi_snapshot().scan_count == 4U);
    assert(restore_page.wifi_snapshot().registration_error ==
           hmi::connectivity::WifiRegistrationError::none);
    restore_page.destroy();
  }

  WifiSettingsPage page;
  assert(page.create(screen, context));
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::list);
  assert(page.request_scan());
  advance(registration, page, now_ms, 5U);
  assert(page.wifi_snapshot().scan_count == 4U);
  assert(page.select_network(0U));
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::editor);
  assert(page.wifi_snapshot().has_selection);
  assert(page.field_text(WifiInputField::ssid) == "HMI-LAB");
  assert(page.wifi_snapshot().active_field == WifiInputField::password);
  assert(page.cancel_edit());
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::list);
  assert(!page.wifi_snapshot().has_selection);
  assert(page.field_text(WifiInputField::ssid).empty());
  // 취소한 AP도 다시 선택해 비밀번호 입력 화면으로 진입할 수 있다.
  assert(page.select_network(0U));
  assert(page.set_locale(hmi::i18n::Locale::en_us));
  assert(page.wifi_snapshot().keyboard_mode ==
         hmi::text::KeyboardMode::latin);
  assert(page.handle_utf8_input("wrongpass"));
  assert(page.field_text(WifiInputField::password).empty());
  assert(page.connect());
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::editor);
  advance(registration, page, now_ms, 5U);
  assert(page.wifi_snapshot().registration_state ==
         WifiRegistrationState::failed);
  assert(page.wifi_snapshot().registration_error ==
         hmi::connectivity::WifiRegistrationError::authentication_failed);
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::editor);
  assert(page.wifi_snapshot().active_field == WifiInputField::password);
  assert(page.field_text(WifiInputField::password).empty());

  // 인증 실패 뒤 같은 SSID에서 올바른 password를 다시 입력하면 실제 IP 확보
  // 전까지 editor를 유지하고, 성공한 뒤에만 목록으로 돌아간다.
  assert(page.handle_utf8_input("starter123"));
  assert(page.connect());
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::editor);
  advance(registration, page, now_ms, 7U);
  assert(page.wifi_snapshot().registration_state ==
         WifiRegistrationState::saved);
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::list);
  assert(page.wifi_snapshot().has_ip);
  assert(page.wifi_snapshot().signal_level == 4U);
  assert(store.save_count() == 1U);

  page.destroy();
  assert(registration.snapshot().state == WifiRegistrationState::saved);
  assert(registration.snapshot().has_ip);

  // 최대 32-byte SSID는 입력 상태에 유지되고, 요청 뒤 비밀번호는 즉시 지워진다.
  assert(page.create(screen, context));
  assert(page.disconnect());
  constexpr std::string_view long_ssid = "12345678901234567890123456789012";
  assert(page.select_field(WifiInputField::ssid));
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::editor);
  assert(!page.wifi_snapshot().has_selection);
  assert(page.handle_utf8_input(long_ssid.data()));
  assert(page.field_text(WifiInputField::ssid) == long_ssid);
  assert(page.select_field(WifiInputField::password));
  assert(page.handle_utf8_input("starter123"));
  assert(page.connect());
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::editor);
  advance(registration, page, now_ms, 6U);
  assert(page.wifi_snapshot().registration_state ==
         WifiRegistrationState::failed);
  assert(page.wifi_snapshot().view == hmi::samples::suite::WifiPageView::editor);
  assert(page.field_text(WifiInputField::password).empty());

  assert(page.request_scan());
  page.destroy();
  assert(registration.scan_snapshot().state ==
         hmi::connectivity::WifiScanState::cancelled);

  lv_obj_del(screen);
  theme.reset();
  return 0;
}
