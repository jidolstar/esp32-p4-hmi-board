#pragma once

#include <lvgl.h>

#include "generated/text_catalog.hpp"
#include "hmi/i18n/locale.hpp"
#include "hmi/ui/theme.hpp"

namespace hmi::samples::suite {

inline constexpr lv_coord_t kPageHeaderHeight = 62;
inline constexpr lv_coord_t kPageOuterPadding = 14;
inline constexpr lv_coord_t kPageSectionGap = 10;

/**
 * sample-suite 화면이 공통으로 사용하는 header의 표시·이벤트 설정이다.
 *
 * callback과 user_data의 수명은 header를 포함한 page root가 삭제될 때까지 호출자가
 * 보장한다. Dashboard처럼 뒤로가기가 필요 없는 화면은 back_event를 비워 둔다.
 */
struct PageHeaderSpec {
  generated::TextId title = generated::TextId::count;
  generated::TextId badge = generated::TextId::count;
  hmi::i18n::Locale locale = hmi::i18n::Locale::ko_kr;
  lv_coord_t title_leading_inset = 0;
  lv_event_cb_t back_event = nullptr;
  lv_event_cb_t locale_ko_event = nullptr;
  lv_event_cb_t locale_en_event = nullptr;
  lv_event_cb_t theme_event = nullptr;
  void* user_data = nullptr;
};

/** 공통 header가 만든 widget 중 page callback에서 식별할 control을 반환한다. */
struct PageHeaderWidgets {
  lv_obj_t* root = nullptr;
  lv_obj_t* back_button = nullptr;
  lv_obj_t* locale_ko_button = nullptr;
  lv_obj_t* locale_en_button = nullptr;
  lv_obj_t* theme_button = nullptr;

  [[nodiscard]] explicit operator bool() const noexcept { return root != nullptr; }
};

/**
 * parent 아래에 고정 높이의 sample-suite 공통 header를 만든다.
 *
 * 반환 widget은 parent가 소유하며 별도로 삭제하지 않는다. 생성 중 실패하면 만든
 * header 전체를 정리하고 빈 결과를 반환한다.
 */
[[nodiscard]] PageHeaderWidgets create_page_header(
    lv_obj_t* parent, const PageHeaderSpec& spec,
    hmi::ui::UiTheme& theme) noexcept;

/** 모든 sample-suite 화면의 바깥 여백과 header/body 간격을 동일하게 설정한다. */
void configure_page_shell_root(lv_obj_t* root,
                               hmi::ui::UiTheme& theme) noexcept;

}  // namespace hmi::samples::suite
