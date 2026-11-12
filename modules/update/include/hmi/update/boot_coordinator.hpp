#pragma once

#include <cstdint>
#include <string_view>

#include "hmi/update/asset_lifecycle.hpp"
#include "hmi/update/install_journal.hpp"

namespace hmi::update {

enum class BootAction : std::uint8_t {
  ready,
  await_health,
  accept,
  rollback,
  recovery,
};

struct BootObservation {
  std::string_view running_generation{};
  bool healthy = false;
  bool asset_binding_valid = false;
  std::uint32_t elapsed_ms = 0U;
  std::uint32_t health_timeout_ms = 30U * 1000U;
};

/** journal과 generation snapshot만 보고 accept/rollback 결정을 반환하는 순수 boot 정책이다. */
class BootCoordinator {
 public:
  [[nodiscard]] BootAction evaluate(
      const InstallJournalSnapshot& journal,
      const AssetGenerationSnapshot& assets,
      const BootObservation& observation) const noexcept;
};

}  // namespace hmi::update
