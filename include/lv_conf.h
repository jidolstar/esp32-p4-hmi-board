#ifndef LV_CONF_H
#define LV_CONF_H

/* Windows와 HMI가 공유할 RGB565 표시 계약이다. */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* 실제 heap은 각 플랫폼 구현이 소유한다. */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "hmi_lvgl_mem.h"
#define LV_MEM_CUSTOM_ALLOC hmi_lvgl_malloc
#define LV_MEM_CUSTOM_FREE hmi_lvgl_free
#define LV_MEM_CUSTOM_REALLOC hmi_lvgl_realloc

#define LV_TICK_CUSTOM 0
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* 첫 진단 화면은 외부 자산 없이 내장 ASCII 글꼴로 열린다. */
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#define LV_USE_FONT_COMPRESSED 1

/* Windows font fixture가 binary font를 읽는 표준 파일 드라이버다.
 * HMI의 SD driver 등록과 asset root 연결은 L5 host 통합에서 담당한다. */
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH ""

#endif  // LV_CONF_H
