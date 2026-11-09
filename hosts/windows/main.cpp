#include <lvgl.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

#include "sdl_lvgl_port.hpp"
#if __has_include("hmi/ui/font_repository.hpp")
#define HMI_WINDOWS_HAS_FONT_REPOSITORY 1
#include "hmi/ui/font_repository.hpp"
#else
#define HMI_WINDOWS_HAS_FONT_REPOSITORY 0
#endif
#include "selected_profile.hpp"
#include "settings/memory_level_settings.hpp"
#if __has_include("hmi/connectivity/wifi_sta_registration.hpp")
#define HMI_WINDOWS_HAS_CONNECTIVITY 1
#include "connectivity/virtual_wifi_sta.hpp"
#include "hmi/connectivity/wifi_sta_registration.hpp"
#else
#define HMI_WINDOWS_HAS_CONNECTIVITY 0
#endif

namespace {

struct Arguments {
  bool smoke = false;
  const char *screenshot_path = nullptr;
};

bool parse_arguments(int count, char **values, Arguments &arguments) {
  for (int index = 1; index < count; ++index) {
    if (std::strcmp(values[index], "--smoke") == 0) {
      arguments.smoke = true;
    } else if (std::strcmp(values[index], "--screenshot") == 0 &&
               index + 1 < count) {
      arguments.screenshot_path = values[++index];
    } else {
      return false;
    }
  }
  return true;
}

struct StdioFontContext {
  const char *profile_root = nullptr;
};

void *load_font(void *context, const char *relative_path,
                std::uint32_t size_bytes, std::uint32_t crc32) noexcept {
#if HMI_WINDOWS_HAS_FONT_REPOSITORY
  auto *font_context = static_cast<StdioFontContext *>(context);
  if (font_context == nullptr || font_context->profile_root == nullptr ||
      relative_path == nullptr) {
    return nullptr;
  }
  std::array<char, 320U> path{};
  const int written = std::snprintf(path.data(), path.size(), "%s%s",
                                    font_context->profile_root, relative_path);
  if (written <= 0 || static_cast<std::size_t>(written) >= path.size()) {
    return nullptr;
  }
  return hmi::ui::load_binary_font_file(path.data(), size_bytes, crc32);
#else
  (void)context;
  (void)relative_path;
  (void)size_bytes;
  (void)crc32;
  return nullptr;
#endif
}

void free_font(void *, void *font) noexcept {
#if HMI_WINDOWS_HAS_FONT_REPOSITORY
  lv_font_free(static_cast<lv_font_t *>(font));
#else
  (void)font;
#endif
}

void handle_utf8_input(void *context, const char *utf8) noexcept {
  auto *app = static_cast<hmi::profiles::generated::SelectedProfileApp *>(context);
  if (app != nullptr) {
    app->handle_utf8_input(utf8);
  }
}

void handle_keyboard_command(
    void *context, hmi::platform::windows::KeyboardCommand command) noexcept {
  auto *app = static_cast<hmi::profiles::generated::SelectedProfileApp *>(context);
  if (app == nullptr) {
    return;
  }
  using hmi::platform::windows::KeyboardCommand;
  using hmi::profiles::AppKey;
  switch (command) {
    case KeyboardCommand::backspace:
      app->handle_key(AppKey::backspace);
      break;
    case KeyboardCommand::enter:
      app->handle_key(AppKey::enter);
      break;
    case KeyboardCommand::cursor_left:
      app->handle_key(AppKey::cursor_left);
      break;
    case KeyboardCommand::cursor_right:
      app->handle_key(AppKey::cursor_right);
      break;
  }
}

}  // namespace

int main(int argc, char **argv) {
  Arguments arguments;
  if (!parse_arguments(argc, argv, arguments)) {
    return 2;
  }

  lv_init();
  int result = 0;
  {
    // draw buffer가 약 80 KiB이므로 큰 port 객체를 main stack에 만들지 않는다.
    auto port = std::make_unique<hmi::platform::windows::SdlLvglPort>();
    const bool visible = !arguments.smoke || arguments.screenshot_path != nullptr;
    if (!port->begin("HMI Starter - Windows", visible)) {
      result = 3;
    } else {
      // 선택 앱이 커져도 Windows main stack 사용량이 늘지 않게 heap에 둔다.
      StdioFontContext font_context{hmi::profiles::generated::kSelectedAssetRoot};
      hmi::platform::settings::MemoryLevelSettings level_settings(
          *port, hmi::profiles::generated::kSelectedAssetRoot);
#if HMI_WINDOWS_HAS_CONNECTIVITY
      hmi::platform::connectivity::VirtualWifiSta wifi_port;
      hmi::platform::connectivity::MemoryWifiStationSettingsStore wifi_store;
      hmi::connectivity::WifiStaRegistration wifi_registration(wifi_port,
                                                               wifi_store);
      const bool wifi_ready = wifi_registration.begin(lv_tick_get(), true);
#endif
      auto app = std::make_unique<hmi::profiles::generated::SelectedProfileApp>();
      hmi::profiles::AppContext app_context;
      app_context.host_name = "windows";
      if constexpr (hmi::profiles::generated::kSelectedHasAssets) {
        app_context.fonts = {&font_context, &load_font, &free_font};
      }
      app_context.level_settings = level_settings.port();
#if HMI_WINDOWS_HAS_CONNECTIVITY
      app_context.wifi_registration = wifi_ready ? &wifi_registration : nullptr;
#endif
      if (!app->create(app_context)) {
        result = 4;
      } else {
        port->set_keyboard_callbacks(
            {app.get(), &handle_utf8_input, &handle_keyboard_command});
      }
      if (result == 0 && (arguments.smoke || arguments.screenshot_path != nullptr)) {
        for (int frame = 0; frame < 3; ++frame) {
          port->tick();
#if HMI_WINDOWS_HAS_CONNECTIVITY
          if (wifi_ready) {
            wifi_registration.tick(lv_tick_get());
          }
#endif
        }
        if (arguments.screenshot_path != nullptr &&
            !port->save_screenshot(arguments.screenshot_path)) {
          result = 5;
        }
      } else if (result == 0) {
        while (port->process_events()) {
          port->tick();
#if HMI_WINDOWS_HAS_CONNECTIVITY
          if (wifi_ready) {
            wifi_registration.tick(lv_tick_get());
          }
#endif
        }
      }
      port->set_keyboard_callbacks({});
    }
  }
  return result;
}
