#include "hmi_lvgl_mem.h"

#include <cstddef>
#include <cstdint>

#include <esp_heap_caps.h>

namespace {

constexpr std::uint32_t kPreferredCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
constexpr std::uint32_t kFallbackCaps = MALLOC_CAP_8BIT;

}  // namespace

extern "C" void *hmi_lvgl_malloc(std::size_t size) {
  if (size == 0U) {
    return nullptr;
  }

  void *pointer = heap_caps_malloc(size, kPreferredCaps);
  if (pointer == nullptr) {
    pointer = heap_caps_malloc(size, kFallbackCaps);
  }
  return pointer;
}

extern "C" void hmi_lvgl_free(void *pointer) { heap_caps_free(pointer); }

extern "C" void *hmi_lvgl_realloc(void *pointer, std::size_t size) {
  if (size == 0U) {
    heap_caps_free(pointer);
    return nullptr;
  }

  void *next = heap_caps_realloc(pointer, size, kPreferredCaps);
  if (next == nullptr) {
    next = heap_caps_realloc(pointer, size, kFallbackCaps);
  }
  return next;
}
