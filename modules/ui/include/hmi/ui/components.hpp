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

}  // namespace hmi::ui
