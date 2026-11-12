#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/update/ports.hpp"

namespace hmi::update {

inline constexpr std::size_t kMaximumDurablePayloadBytes = 128U;

struct DurableRecord {
  std::uint32_t sequence = 0U;
  std::uint16_t size = 0U;
  std::uint8_t payload[kMaximumDurablePayloadBytes]{};
};

/**
 * sequence와 CRC가 유효한 최신 A/B slot을 선택하는 비소유 저장 조정기다.
 *
 * port owner는 store보다 오래 살아야 한다. `save()`는 반대 slot 기록·flush·readback 확인이
 * 끝난 뒤에만 새 sequence를 반환한다.
 */
class DurableRecordStore {
 public:
  explicit DurableRecordStore(DurableRecordPort port) noexcept : port_(port) {}

  [[nodiscard]] Error load(DurableRecord& output) const noexcept;
  [[nodiscard]] Error save(const std::uint8_t* payload, std::size_t size,
                           std::uint32_t current_sequence,
                           std::uint32_t& saved_sequence) const noexcept;

 private:
  [[nodiscard]] bool read_slot(std::uint8_t slot,
                               DurableRecord& output) const noexcept;

  DurableRecordPort port_{};
};

static_assert(sizeof(DurableRecord) <= 136U,
              "durable payload must remain bounded");
static_assert(sizeof(DurableRecordStore) <= 40U,
              "durable store must keep one non-owning port");

}  // namespace hmi::update
