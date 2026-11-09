#pragma once

#include <cstdint>
#include <cstring>
#include <limits>

namespace hmi::boards::jc1060p470c::internal {

inline constexpr std::uint32_t kMscSectorSize = 512U;

/** USB MSC의 offset/length 요청을 bounded 512-byte SD raw sector 연산으로 변환한다. */
template <typename ReadSector, typename WriteSector>
std::int32_t transfer_msc_sectors(bool writing, std::uint32_t lba,
                                  std::uint32_t offset, void* buffer,
                                  std::uint32_t size, ReadSector read_sector,
                                  WriteSector write_sector) {
  if (size == 0U) {
    return 0;
  }
  if (buffer == nullptr ||
      size > static_cast<std::uint32_t>(
                 std::numeric_limits<std::int32_t>::max())) {
    return -1;
  }

  // 범위를 먼저 검증해 overflow 요청이 일부 sector만 변경하는 상황을 막는다.
  const std::uint64_t last_position =
      static_cast<std::uint64_t>(offset) + size - 1U;
  const std::uint64_t last_block =
      static_cast<std::uint64_t>(lba) + last_position / kMscSectorSize;
  if (last_block > std::numeric_limits<std::uint32_t>::max()) {
    return -1;
  }

  // callback stack에는 정렬된 sector buffer 하나만 두고 heap을 사용하지 않는다.
  alignas(4) std::uint8_t sector[kMscSectorSize]{};
  auto* bytes = static_cast<std::uint8_t*>(buffer);
  std::uint64_t position = offset;
  std::uint32_t processed = 0U;
  while (processed < size) {
    const std::uint64_t block =
        static_cast<std::uint64_t>(lba) + position / kMscSectorSize;
    const auto within =
        static_cast<std::uint32_t>(position % kMscSectorSize);
    const auto available = kMscSectorSize - within;
    const auto remaining = size - processed;
    const auto chunk = remaining < available ? remaining : available;
    const auto block_number = static_cast<std::uint32_t>(block);

    if (writing) {
      if ((within != 0U || chunk != kMscSectorSize) &&
          !read_sector(sector, block_number)) {
        return -1;
      }
      std::memcpy(sector + within, bytes + processed, chunk);
      if (!write_sector(sector, block_number)) {
        return -1;
      }
    } else {
      if (!read_sector(sector, block_number)) {
        return -1;
      }
      std::memcpy(bytes + processed, sector + within, chunk);
    }
    position += chunk;
    processed += chunk;
  }
  return static_cast<std::int32_t>(processed);
}

}  // namespace hmi::boards::jc1060p470c::internal
