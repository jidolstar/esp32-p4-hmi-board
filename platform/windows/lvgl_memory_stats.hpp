#pragma once

#include <cstddef>

namespace hmi::platform::windows {

/** Windows CRT가 현재와 최대 시점에 보유한 LVGL heap byte 수다. */
struct LvglMemoryStats {
  std::size_t current_bytes = 0U;
  std::size_t peak_bytes = 0U;
};

/** UI thread에서 allocator 진단용으로만 호출한다. */
LvglMemoryStats lvgl_memory_stats() noexcept;

}  // namespace hmi::platform::windows
