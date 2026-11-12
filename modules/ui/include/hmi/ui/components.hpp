#pragma once

#include <lvgl.h>

#include "hmi/ui/theme.hpp"

namespace hmi::ui {

/** 공통 테마가 적용된 label을 만들며 반환된 widget의 수명은 parent가 소유한다. */
lv_obj_t *create_label(lv_obj_t *parent, const char *text, UiTheme &theme,
                       TextRole role) noexcept;

/** 공통 테마와 event callback이 적용된 button을 만든다. */
lv_obj_t *create_button(lv_obj_t *parent, const char *text, UiTheme &theme,
                        ButtonVariant variant, lv_event_cb_t callback,
                        void *user_data) noexcept;

/** 여러 줄 label의 폭, 높이, 줄 간격을 명시적으로 고정한다. */
void configure_multiline(lv_obj_t *label, lv_coord_t width, lv_coord_t height,
                         lv_coord_t line_space,
                         lv_text_align_t align = LV_TEXT_ALIGN_LEFT) noexcept;

/** 문구는 LVGL이 복사한다. 지정한 글꼴은 overlay가 닫힐 때까지 유효해야 한다. */
struct OverlayText {
  const char* text = nullptr;
  const lv_font_t* font = nullptr;
};

struct ConfirmationSpec {
  OverlayText title;
  OverlayText body;
  OverlayText cancel;
  OverlayText confirm;
  lv_event_cb_t on_cancel = nullptr;
  lv_event_cb_t on_confirm = nullptr;
  void* context = nullptr;
};

/** 화면이 소유하는 modal 위젯이다. 호출자가 화면 또는 테마를 교체하기 전에 닫는다. */
struct ConfirmationWidgets {
  lv_obj_t* scrim = nullptr;
  lv_obj_t* cancel = nullptr;
  lv_obj_t* confirm = nullptr;

  [[nodiscard]] explicit operator bool() const noexcept { return scrim != nullptr; }
  void close() noexcept;
};

/** anchor가 속한 화면에 modal을 띄운다. cancel 문구를 비우면 단일 확인 버튼을 표시한다. */
[[nodiscard]] ConfirmationWidgets show_confirmation(
    lv_obj_t* anchor, UiTheme& theme, const ConfirmationSpec& spec) noexcept;

/** 화면 위의 입력을 통과시키는 toast와 자동 종료 timer를 함께 소유한다. */
class Toast {
 public:
  using OnExpired = void (*)(void*) noexcept;
  Toast() = default;
  Toast(const Toast&) = delete;
  Toast& operator=(const Toast&) = delete;
  ~Toast() { dismiss(); }

  /** 화면이나 테마를 교체하기 전에 dismiss를 호출한다. */
  bool show(lv_obj_t* anchor, UiTheme& theme, OverlayText message,
            std::uint32_t duration_ms, OnExpired on_expired, void* context) noexcept;
  void dismiss() noexcept;

 private:
  static void expire(lv_timer_t* timer);
  lv_obj_t* panel_ = nullptr;
  lv_timer_t* timer_ = nullptr;
  OnExpired on_expired_ = nullptr;
  void* context_ = nullptr;
};

}  // namespace hmi::ui
