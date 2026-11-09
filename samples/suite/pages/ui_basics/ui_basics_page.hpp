#pragma once

#include <lvgl.h>

#include "page_contract.hpp"

namespace hmi::samples::suite {

/**
 * 기본 pointer 동작, scroll, toast와 confirm dialog를 보여 주는 진단 page다.
 *
 * root 아래의 page widget, content 직계 자식인 dialog overlay와 toast
 * timer를 소유한다. destroy는 timer·overlay를 먼저 정리하고 root를
 * 삭제하므로 Dashboard로 돌아간 뒤 callback 대상이 남지 않는다.
 */
class UiBasicsPage final : public SamplePage {
 public:
  UiBasicsPage() = default;
  UiBasicsPage(const UiBasicsPage &) = delete;
  UiBasicsPage &operator=(const UiBasicsPage &) = delete;

  bool create(lv_obj_t *parent, const PageContext &context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override;
  ~UiBasicsPage() override;

  void record_press() noexcept;
  void record_long_press() noexcept;
  bool show_toast() noexcept;
  bool open_dialog() noexcept;
  void resolve_dialog(bool confirmed) noexcept;
  void expire_toast() noexcept;

 private:
  static void handle_event(lv_event_t *event);
  static void handle_toast_timer(lv_timer_t *timer);
  void update_status() noexcept;
  void clear_overlay() noexcept;

  PageContext context_{};
  PageSnapshot state_{};
  lv_obj_t *root_ = nullptr;
  lv_obj_t *status_label_ = nullptr;
  lv_obj_t *back_button_ = nullptr;
  lv_obj_t *locale_ko_button_ = nullptr;
  lv_obj_t *locale_en_button_ = nullptr;
  lv_obj_t *theme_button_ = nullptr;
  lv_obj_t *press_button_ = nullptr;
  lv_obj_t *hold_button_ = nullptr;
  lv_obj_t *toast_button_ = nullptr;
  lv_obj_t *dialog_button_ = nullptr;
  lv_obj_t *dialog_cancel_button_ = nullptr;
  lv_obj_t *dialog_confirm_button_ = nullptr;
  lv_obj_t *overlay_ = nullptr;
  lv_timer_t *toast_timer_ = nullptr;
};

}  // namespace hmi::samples::suite
