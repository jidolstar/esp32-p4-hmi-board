#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "hmi/update/install_journal.hpp"

namespace {

struct StorageFixture {
  std::array<std::array<std::uint8_t, hmi::update::kDurableRecordBytes>, 2U>
      slots{};
  std::size_t sizes[2]{};
  bool fail_write = false;
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
  if (fixture.fail_write) {
    fixture.fail_write = false;
    return hmi::update::Error::storage;
  }
  std::memcpy(fixture.slots[slot].data(), input, size);
  fixture.sizes[slot] = size;
  return hmi::update::Error::none;
}

hmi::update::Error flush(void*) { return hmi::update::Error::none; }

hmi::update::DurableRecordStore store(StorageFixture& fixture) {
  return hmi::update::DurableRecordStore({&fixture, &read, &write, &flush});
}

}  // namespace

int main() {
  using namespace hmi::update;
  constexpr std::string_view kFirst =
      "11111111-1111-1111-1111-111111111111";
  constexpr std::string_view kSecond =
      "22222222-2222-2222-2222-222222222222";

  StorageFixture fixture;
  auto durable = store(fixture);
  InstallJournal journal(durable);
  assert(journal.recover() == Error::none);
  assert(journal.snapshot().stage == InstallStage::idle);
  assert(journal.begin(kFirst) == Error::none);
  assert(journal.begin(kSecond) == Error::busy);
  assert(journal.transition(InstallStage::verified) == Error::invalid_argument);
  assert(journal.transition(InstallStage::downloading) == Error::none);
  assert(journal.snapshot().sequence == 2U);

  // 최신 slot을 손상시키면 이전의 valid record로 복구해야 한다.
  fixture.slots[0][0] ^= 0x55U;
  InstallJournal recovered(durable);
  assert(recovered.recover() == Error::none);
  assert(recovered.snapshot().stage == InstallStage::prepared);
  assert(recovered.snapshot().sequence == 1U);

  fixture.fail_write = true;
  assert(recovered.transition(InstallStage::downloading) == Error::storage);
  assert(recovered.snapshot().stage == InstallStage::prepared);
  assert(recovered.transition(InstallStage::downloading) == Error::none);
  assert(recovered.transition(InstallStage::verified) == Error::none);
  assert(recovered.transition(InstallStage::installing) == Error::none);
  assert(recovered.transition(InstallStage::awaiting_boot) == Error::none);
  assert(recovered.transition(InstallStage::confirmed) == Error::none);
  assert(!recovered.active());
  assert(recovered.begin(kSecond) == Error::none);
  assert(recovered.transition(InstallStage::failed, Error::network) ==
         Error::none);
  assert(recovered.snapshot().last_error == Error::network);

  return 0;
}
