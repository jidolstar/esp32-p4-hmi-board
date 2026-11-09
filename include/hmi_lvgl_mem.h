#ifndef HMI_LVGL_MEM_H
#define HMI_LVGL_MEM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 플랫폼별 LVGL heap에 size byte를 할당한다. size가 0이면 NULL을 반환한다. */
void *hmi_lvgl_malloc(size_t size);
/** hmi_lvgl_malloc 또는 hmi_lvgl_realloc이 반환한 pointer를 해제한다. */
void hmi_lvgl_free(void *pointer);
/** 기존 내용을 유지하며 size byte로 변경한다. size가 0이면 해제 후 NULL을 반환한다. */
void *hmi_lvgl_realloc(void *pointer, size_t size);

#ifdef __cplusplus
}
#endif

#endif  // HMI_LVGL_MEM_H
