#pragma once

#include <cstdint>

#include "hmi/update/durable_record.hpp"

namespace hmi::update {

enum class InstallStage : std::uint8_t {
  idle,
  prepared,
  downloading,
  verified,
  installing,
  awaiting_boot,
  confirmed,
  rolling_back,
  rolled_back,
  failed,
  cancelled,
};

struct InstallJournalSnapshot {
  char installation_id[37]{};
  std::uint32_t sequence = 0U;
  InstallStage stage = InstallStage::idle;
  Error last_error = Error::none;
};

/** update stage를 매 전이마다 durable A/B record에 남기는 작은 상태기계다. */
class InstallJournal {
 public:
  explicit InstallJournal(DurableRecordStore& store) noexcept : store_(store) {}

  [[nodiscard]] Error recover() noexcept;
  [[nodiscard]] Error begin(std::string_view installation_id) noexcept;
  [[nodiscard]] Error transition(InstallStage next,
                                 Error error = Error::none) noexcept;
  [[nodiscard]] InstallJournalSnapshot snapshot() const noexcept {
    return snapshot_;
  }
  [[nodiscard]] bool active() const noexcept;

 private:
  [[nodiscard]] Error persist(const InstallJournalSnapshot& value) noexcept;

  DurableRecordStore& store_;
  InstallJournalSnapshot snapshot_{};
};

static_assert(sizeof(InstallJournalSnapshot) <= 48U,
              "install journal snapshot must remain bounded");
static_assert(sizeof(InstallJournal) <= 64U,
              "install journal must keep one store reference and scalar state");

}  // namespace hmi::update
