#include "hmi_lvgl_mem.h"
#include "lvgl_memory_stats.hpp"

#include <cassert>

int main() {
  using hmi::platform::windows::lvgl_memory_stats;

  const auto initial = lvgl_memory_stats();
  assert(hmi_lvgl_malloc(0U) == nullptr);

  void *pointer = hmi_lvgl_malloc(64U);
  assert(pointer != nullptr);
  const auto allocated = lvgl_memory_stats();
  assert(allocated.current_bytes >= initial.current_bytes + 64U);
  assert(allocated.peak_bytes >= allocated.current_bytes);

  pointer = hmi_lvgl_realloc(pointer, 256U);
  assert(pointer != nullptr);
  const auto resized = lvgl_memory_stats();
  assert(resized.current_bytes >= initial.current_bytes + 256U);
  assert(resized.peak_bytes >= resized.current_bytes);

  pointer = hmi_lvgl_realloc(pointer, 0U);
  assert(pointer == nullptr);
  const auto released = lvgl_memory_stats();
  assert(released.current_bytes == initial.current_bytes);

  hmi_lvgl_free(nullptr);
  assert(lvgl_memory_stats().current_bytes == initial.current_bytes);
  return 0;
}
