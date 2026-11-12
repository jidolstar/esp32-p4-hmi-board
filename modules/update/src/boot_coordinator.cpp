#include "hmi/update/boot_coordinator.hpp"

namespace hmi::update {

BootAction BootCoordinator::evaluate(
    const InstallJournalSnapshot& journal,
    const AssetGenerationSnapshot& assets,
    const BootObservation& observation) const noexcept {
  if (journal.stage == InstallStage::confirmed ||
      journal.stage == InstallStage::rolled_back ||
      journal.stage == InstallStage::idle) {
    return BootAction::ready;
  }
  if (journal.stage == InstallStage::rolling_back) {
    return observation.running_generation == assets.active
               ? BootAction::ready
               : BootAction::recovery;
  }
  if (journal.stage != InstallStage::awaiting_boot ||
      assets.pending[0] == '\0') {
    return BootAction::recovery;
  }
  if (observation.running_generation != assets.pending) {
    return BootAction::recovery;
  }
  if (!observation.asset_binding_valid ||
      observation.elapsed_ms >= observation.health_timeout_ms) {
    return BootAction::rollback;
  }
  return observation.healthy ? BootAction::accept
                             : BootAction::await_health;
}

}  // namespace hmi::update
