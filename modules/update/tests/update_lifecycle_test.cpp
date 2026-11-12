#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "hmi/update/asset_lifecycle.hpp"
#include "hmi/update/asset_store.hpp"

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

bool visit(void* context, std::string_view generation, bool, bool, bool) {
  auto& count = *static_cast<unsigned*>(context);
  ++count;
  return generation == "33333333-3333-3333-3333-333333333333";
}

}  // namespace

int main() {
  using namespace hmi::update;
  constexpr std::string_view kFirst =
      "11111111-1111-1111-1111-111111111111";
  constexpr std::string_view kSecond =
      "22222222-2222-2222-2222-222222222222";
  constexpr std::string_view kThird =
      "33333333-3333-3333-3333-333333333333";

  StorageFixture fixture;
  DurableRecordStore durable({&fixture, &read, &write, &flush});
  AssetLifecycle assets(durable);
  assert(assets.recover() == Error::none);
  assert(assets.adopt_active(kFirst) == Error::none);
  assert(assets.prepare(kSecond) == Error::none);
  assert(assets.prepare(kThird) == Error::busy);

  AssetLifecycle recovered(durable);
  assert(recovered.recover() == Error::none);
  assert(std::string_view(recovered.snapshot().active) == kFirst);
  assert(std::string_view(recovered.snapshot().pending) == kSecond);
  assert(recovered.activate_pending() == Error::none);
  assert(std::string_view(recovered.snapshot().active) == kSecond);
  assert(std::string_view(recovered.snapshot().previous) == kFirst);
  assert(recovered.rollback_active() == Error::none);
  assert(std::string_view(recovered.snapshot().active) == kFirst);

  const std::string_view generations[] = {kFirst, kSecond, kThird};
  unsigned visited = 0U;
  const auto snapshot = recovered.snapshot();
  assert(visit_stale_generations(generations, 3U, snapshot.active,
                                 snapshot.previous, snapshot.pending, 3U,
                                 &visit, &visited) == Error::none);
  assert(visited == 1U);
  assert(visit_stale_generations(generations, 3U, snapshot.active,
                                 snapshot.previous, snapshot.pending, 2U,
                                 &visit, &visited) == Error::invalid_argument);

  return 0;
}
