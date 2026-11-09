#pragma once

#include <cstdint>

namespace hmi::boards::jc1060p470c {

enum class AssetUpdatePhase : std::uint8_t {
  idle,
  preparing,
  receiving,
  completed,
  failed,
};

enum class AssetUpdatePreparationStep : std::uint8_t {
  none,
  fonts,
  assets,
};

/**
 * USB 자산 갱신기의 UI 독립 progress snapshot이다.
 *
 * Board가 값으로 반환하며 호출자는 보관된 내부 포인터를 받지 않는다. bytes_received는
 * SD에 아직 flush되지 않은 bounded window까지 포함하며 total_bytes를 넘지 않는다.
 */
struct AssetUpdateStatus {
  AssetUpdatePhase phase = AssetUpdatePhase::idle;
  AssetUpdatePreparationStep preparation = AssetUpdatePreparationStep::none;
  std::uint16_t file_index = 0U;
  std::uint16_t file_count = 0U;
  std::uint32_t bytes_received = 0U;
  std::uint32_t total_bytes = 0U;
};

static_assert(sizeof(AssetUpdateStatus) <= 16U,
              "Asset update polling must use a small value snapshot");

}  // namespace hmi::boards::jc1060p470c
