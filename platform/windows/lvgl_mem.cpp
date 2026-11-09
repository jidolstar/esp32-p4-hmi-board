#include "hmi_lvgl_mem.h"

#include <algorithm>
#include <cstdlib>
#include <malloc.h>

#include "lvgl_memory_stats.hpp"

namespace {

std::size_t held_bytes = 0U;
std::size_t peak_bytes = 0U;

std::size_t allocation_size(void *pointer) noexcept {
  return pointer == nullptr ? 0U : _msize(pointer);
}

void add_allocation(void *pointer) noexcept {
  held_bytes += allocation_size(pointer);
  peak_bytes = std::max(peak_bytes, held_bytes);
}

}  // namespace

namespace hmi::platform::windows {

LvglMemoryStats lvgl_memory_stats() noexcept {
  return {held_bytes, peak_bytes};
}

}  // namespace hmi::platform::windows

extern "C" void *hmi_lvgl_malloc(size_t size) {
  if (size == 0U) {
    return nullptr;
  }
  void *pointer = std::malloc(size);
  add_allocation(pointer);
  return pointer;
}

extern "C" void hmi_lvgl_free(void *pointer) {
  held_bytes -= allocation_size(pointer);
  std::free(pointer);
}

extern "C" void *hmi_lvgl_realloc(void *pointer, size_t size) {
  if (size == 0U) {
    hmi_lvgl_free(pointer);
    return nullptr;
  }

  const std::size_t previous_size = allocation_size(pointer);
  void *next = std::realloc(pointer, size);
  if (next != nullptr) {
    held_bytes -= previous_size;
    add_allocation(next);
  }
  return next;
}
