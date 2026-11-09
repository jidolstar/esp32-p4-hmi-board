#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

#include "boards/jc1060p470c/internal/msc_sector_transfer.hpp"

namespace {

using hmi::boards::jc1060p470c::internal::kMscSectorSize;
using hmi::boards::jc1060p470c::internal::transfer_msc_sectors;

struct FakeDisk {
  std::array<std::array<std::uint8_t, kMscSectorSize>, 4U> sectors{};
  unsigned reads = 0U;
  unsigned writes = 0U;

  bool read(std::uint8_t* output, std::uint32_t block) {
    ++reads;
    if (output == nullptr || block >= sectors.size()) {
      return false;
    }
    std::memcpy(output, sectors[block].data(), kMscSectorSize);
    return true;
  }

  bool write(const std::uint8_t* input, std::uint32_t block) {
    ++writes;
    if (input == nullptr || block >= sectors.size()) {
      return false;
    }
    std::memcpy(sectors[block].data(), input, kMscSectorSize);
    return true;
  }
};

template <typename Buffer>
std::int32_t transfer(FakeDisk& disk, bool writing, std::uint32_t lba,
                      std::uint32_t offset, Buffer& buffer,
                      std::uint32_t size) {
  return transfer_msc_sectors(
      writing, lba, offset, buffer.data(), size,
      [&disk](std::uint8_t* sector, std::uint32_t block) {
        return disk.read(sector, block);
      },
      [&disk](const std::uint8_t* sector, std::uint32_t block) {
        return disk.write(sector, block);
      });
}

}  // namespace

int main() {
  FakeDisk disk;
  for (std::size_t block = 0U; block < disk.sectors.size(); ++block) {
    disk.sectors[block].fill(static_cast<std::uint8_t>(block + 1U));
  }

  std::array<std::uint8_t, 32U> partial{};
  partial.fill(0xA5U);
  assert(transfer(disk, true, 1U, 500U, partial,
                  static_cast<std::uint32_t>(partial.size())) == 32);
  assert(disk.sectors[1][499] == 2U);
  assert(disk.sectors[1][500] == 0xA5U);
  assert(disk.sectors[2][19] == 0xA5U);
  assert(disk.sectors[2][20] == 3U);

  std::array<std::uint8_t, 1024U> full{};
  assert(transfer(disk, false, 1U, 0U, full,
                  static_cast<std::uint32_t>(full.size())) == 1024);
  assert(full[500] == 0xA5U);
  assert(full[512U + 19U] == 0xA5U);

  FakeDisk aligned_disk;
  std::array<std::uint8_t, kMscSectorSize> aligned{};
  aligned.fill(0x7CU);
  assert(transfer(aligned_disk, true, 0U, 0U, aligned,
                  kMscSectorSize) ==
         static_cast<std::int32_t>(kMscSectorSize));
  assert(aligned_disk.reads == 0U);
  assert(aligned_disk.writes == 1U);

  const auto before = aligned_disk.sectors[0];
  std::array<std::uint8_t, 2U> overflow{{1U, 2U}};
  assert(transfer(aligned_disk, true,
                  std::numeric_limits<std::uint32_t>::max(), 511U,
                  overflow, 2U) == -1);
  assert(aligned_disk.sectors[0] == before);
  assert(transfer_msc_sectors(
             false, 0U, 0U, nullptr, 1U,
             [](std::uint8_t*, std::uint32_t) { return true; },
             [](const std::uint8_t*, std::uint32_t) { return true; }) == -1);
  assert(transfer_msc_sectors(
             false, 0U, 0U, nullptr, 0U,
             [](std::uint8_t*, std::uint32_t) { return true; },
             [](const std::uint8_t*, std::uint32_t) { return true; }) == 0);
  return 0;
}
