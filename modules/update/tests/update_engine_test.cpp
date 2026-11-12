#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "hmi/update/engine.hpp"

namespace {

struct StorageFixture {
  std::array<std::array<std::uint8_t, hmi::update::kDurableRecordBytes>, 2U>
      slots{};
  std::size_t sizes[2]{};
};

hmi::update::Error read(void* context, std::uint8_t slot,
                        std::uint8_t* output, std::size_t capacity,
                        std::size_t& used) {
  auto& fixture = *static_cast<StorageFixture*>(context);
  if (slot > 1U || fixture.sizes[slot] == 0U) {
    used = 0U;
    return hmi::update::Error::unavailable;
  }
  if (capacity < fixture.sizes[slot]) {
    return hmi::update::Error::storage;
  }
  used = fixture.sizes[slot];
  std::memcpy(output, fixture.slots[slot].data(), used);
  return hmi::update::Error::none;
}

hmi::update::Error write(void* context, std::uint8_t slot,
                         const std::uint8_t* input, std::size_t size) {
  auto& fixture = *static_cast<StorageFixture*>(context);
  std::memcpy(fixture.slots[slot].data(), input, size);
  fixture.sizes[slot] = size;
  return hmi::update::Error::none;
}

hmi::update::Error flush(void*) { return hmi::update::Error::none; }

hmi::update::DurableRecordPort port(StorageFixture& fixture) {
  return {&fixture, &read, &write, &flush};
}

}  // namespace

int main() {
  using namespace hmi::update;
  constexpr char kSource[] = "11111111-1111-1111-1111-111111111111";
  constexpr char kTarget[] = "22222222-2222-2222-2222-222222222222";

  StorageFixture journal_storage;
  StorageFixture asset_storage;
  DurableRecordStore journal_records(port(journal_storage));
  DurableRecordStore asset_records(port(asset_storage));
  InstallJournal journal(journal_records);
  AssetLifecycle assets(asset_records);
  MaintenanceGate gate;
  Engine engine(gate, journal, assets);

  assert(journal.recover() == Error::none);
  assert(assets.recover() == Error::none);
  assert(assets.adopt_active(kSource) == Error::none);

  Manifest manifest{};
  std::strcpy(manifest.installation_id, kTarget);
  manifest.target = Target::firmware;
  assert(engine.prepare(manifest) == Error::none);
  assert(gate.active());
  assert(engine.prepare(manifest) == Error::busy);
  assert(engine.mark_download_started() == Error::none);
  assert(engine.mark_verified() == Error::none);
  assert(engine.mark_installing() == Error::none);
  assert(engine.mark_awaiting_boot() == Error::none);
  assert(journal.snapshot().stage == InstallStage::awaiting_boot);
  assert(engine.confirm_boot() == Error::none);
  assert(journal.snapshot().stage == InstallStage::confirmed);
  assert(std::string_view(assets.snapshot().active) == kTarget);
  assert(!gate.active());

  return 0;
}
