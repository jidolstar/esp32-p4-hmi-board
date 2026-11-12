#include <cassert>
#include <cstring>

#include "hmi/update/boot_coordinator.hpp"

int main() {
  using namespace hmi::update;
  constexpr char kSource[] = "11111111-1111-1111-1111-111111111111";
  constexpr char kTarget[] = "22222222-2222-2222-2222-222222222222";

  InstallJournalSnapshot journal{};
  std::strcpy(journal.installation_id, kTarget);
  journal.stage = InstallStage::awaiting_boot;

  AssetGenerationSnapshot assets{};
  std::strcpy(assets.active, kSource);
  std::strcpy(assets.pending, kTarget);

  BootCoordinator coordinator;
  BootObservation observation{kTarget, false, true, 1000U, 30000U};
  assert(coordinator.evaluate(journal, assets, observation) ==
         BootAction::await_health);

  observation.healthy = true;
  assert(coordinator.evaluate(journal, assets, observation) ==
         BootAction::accept);

  observation.asset_binding_valid = false;
  assert(coordinator.evaluate(journal, assets, observation) ==
         BootAction::rollback);

  observation.asset_binding_valid = true;
  observation.healthy = false;
  observation.elapsed_ms = 30000U;
  assert(coordinator.evaluate(journal, assets, observation) ==
         BootAction::rollback);

  observation.running_generation = kSource;
  assert(coordinator.evaluate(journal, assets, observation) ==
         BootAction::recovery);

  journal.stage = InstallStage::confirmed;
  assert(coordinator.evaluate(journal, assets, observation) ==
         BootAction::ready);

  journal.stage = InstallStage::installing;
  assert(coordinator.evaluate(journal, assets, observation) ==
         BootAction::recovery);

  return 0;
}
