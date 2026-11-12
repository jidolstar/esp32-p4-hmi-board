#pragma once

#include <cstdint>

namespace hmi::boards::jc1060p470c { class Board; }
namespace hmi::storage { class StorageAccessController; }

namespace hmi::diagnostics::camera_gallery {

bool begin(boards::jc1060p470c::Board& board, storage::StorageAccessController& storage,
           const char* root) noexcept;
void tick(std::uint32_t now_ms) noexcept;

}  // namespace hmi::diagnostics::camera_gallery
