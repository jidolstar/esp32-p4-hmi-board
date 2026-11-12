#pragma once

#include <cstdint>

#include <lvgl.h>

#include "hmi/connectivity/wifi_ap_web_controller.hpp"
#include "hmi/ui/components.hpp"
#include "page_contract.hpp"

namespace hmi::samples::suite {

/**
 * AP 연결 안내, 두 QR 단계와 네 canonical switch를 한 화면에서 조립한다.
 *
 * controller는 host가 소유한다. locale/theme rebuild의 destroy는 widget만 제거하고 AP
 * session은 유지하며, page 객체의 최종 소멸에서만 shutdown fallback을 수행한다.
 */
class WifiApWebPage final : public SamplePage {
 public:
  WifiApWebPage() = default;
  WifiApWebPage(const WifiApWebPage&) = delete;
  WifiApWebPage& operator=(const WifiApWebPage&) = delete;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override { return {}; }
  ~WifiApWebPage() override;

 private:
  enum class VisualState : std::uint8_t {
    invalid,
    off,
    working,
    waiting_wifi,
    waiting_web,
    controlling,
    reconnecting,
    error,
  };

  static void handle_back(lv_event_t* event);
  static void handle_primary(lv_event_t* event);
  static void handle_secondary(lv_event_t* event);
  static void handle_stop(lv_event_t* event);
  static void handle_switch(lv_event_t* event);
  static void handle_locale_ko(lv_event_t* event);
  static void handle_locale_en(lv_event_t* event);
  static void handle_theme(lv_event_t* event);
  static void handle_confirm_cancel(lv_event_t* event);
  static void handle_confirm_stop(lv_event_t* event);
  static void handle_timer(lv_timer_t* timer) noexcept;

  [[nodiscard]] VisualState resolve_visual_state() noexcept;
  bool rebuild_body() noexcept;
  bool build_onboarding(VisualState state) noexcept;
  bool build_active(VisualState state) noexcept;
  void update_switches() noexcept;
  void show_stop_confirmation(bool leave_after_stop) noexcept;
  void close_confirmation() noexcept;

  PageContext context_{};
  hmi::connectivity::WifiApWebController* controller_ = nullptr;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* body_ = nullptr;
  lv_obj_t* status_label_ = nullptr;
  lv_obj_t* message_label_ = nullptr;
  lv_obj_t* qr_ = nullptr;
  lv_obj_t* switch_cards_[hmi::connectivity::kWifiApWebSwitchCount]{};
  lv_obj_t* switch_value_labels_[hmi::connectivity::kWifiApWebSwitchCount]{};
  hmi::ui::ConfirmationWidgets confirmation_{};
  lv_timer_t* timer_ = nullptr;
  VisualState visual_state_ = VisualState::invalid;
  bool ever_web_connected_ = false;
  bool show_wifi_qr_ = false;
  bool leave_on_confirm_ = false;
  bool leave_after_stop_ = false;
};

static_assert(sizeof(WifiApWebPage) <= 512U,
              "S07 page keeps only widget handles and bounded UI flags");

}  // namespace hmi::samples::suite
