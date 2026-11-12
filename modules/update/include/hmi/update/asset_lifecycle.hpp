#pragma once

#include <cstdint>
#include <string_view>

#include "hmi/update/durable_record.hpp"

namespace hmi::update {

struct AssetGenerationSnapshot {
  char active[37]{};
  char previous[37]{};
  char pending[37]{};
  std::uint32_t sequence = 0U;
};

/** active/previous/pending generation의 원자적 선택 정책을 소유한다. 실제 파일은 adapter가 소유한다. */
class AssetLifecycle {
 public:
  explicit AssetLifecycle(DurableRecordStore& store) noexcept : store_(store) {}

  [[nodiscard]] Error recover() noexcept;
  [[nodiscard]] Error adopt_active(std::string_view generation) noexcept;
  [[nodiscard]] Error prepare(std::string_view generation) noexcept;
  [[nodiscard]] Error activate_pending() noexcept;
  [[nodiscard]] Error discard_pending() noexcept;
  [[nodiscard]] Error rollback_active() noexcept;
  [[nodiscard]] AssetGenerationSnapshot snapshot() const noexcept {
    return snapshot_;
  }

 private:
  [[nodiscard]] Error persist(const AssetGenerationSnapshot& value) noexcept;

  DurableRecordStore& store_;
  AssetGenerationSnapshot snapshot_{};
};

static_assert(sizeof(AssetGenerationSnapshot) <= 120U,
              "three generation identities must fit one durable payload");
static_assert(sizeof(AssetLifecycle) <= 136U,
              "asset lifecycle must keep bounded generation metadata");

}  // namespace hmi::update
