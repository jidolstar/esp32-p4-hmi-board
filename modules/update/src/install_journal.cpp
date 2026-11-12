#include "hmi/update/install_journal.hpp"

#include <cstring>

namespace hmi::update {
namespace {

constexpr std::size_t kPayloadBytes = 39U;

void encode(const InstallJournalSnapshot& value,
            std::uint8_t (&output)[kPayloadBytes]) noexcept {
  std::memset(output, 0, sizeof(output));
  std::memcpy(output, value.installation_id, 37U);
  output[37] = static_cast<std::uint8_t>(value.stage);
  output[38] = static_cast<std::uint8_t>(value.last_error);
}

bool decode(const DurableRecord& record,
            InstallJournalSnapshot& output) noexcept {
  if (record.size != kPayloadBytes || record.payload[36] != 0U ||
      static_cast<InstallStage>(record.payload[37]) > InstallStage::cancelled ||
      static_cast<Error>(record.payload[38]) > Error::paused) {
    return false;
  }
  InstallJournalSnapshot parsed{};
  std::memcpy(parsed.installation_id, record.payload, 37U);
  parsed.sequence = record.sequence;
  parsed.stage = static_cast<InstallStage>(record.payload[37]);
  parsed.last_error = static_cast<Error>(record.payload[38]);
  if (parsed.stage != InstallStage::idle &&
      !valid_uuid(parsed.installation_id)) {
    return false;
  }
  output = parsed;
  return true;
}

bool terminal(InstallStage stage) noexcept {
  return stage == InstallStage::idle || stage == InstallStage::confirmed ||
         stage == InstallStage::rolled_back || stage == InstallStage::failed ||
         stage == InstallStage::cancelled;
}

bool allowed(InstallStage current, InstallStage next) noexcept {
  if (current == InstallStage::prepared) {
    return next == InstallStage::downloading;
  }
  if (current == InstallStage::downloading) {
    return next == InstallStage::verified;
  }
  if (current == InstallStage::verified) {
    return next == InstallStage::installing;
  }
  if (current == InstallStage::installing) {
    return next == InstallStage::awaiting_boot;
  }
  if (current == InstallStage::awaiting_boot) {
    return next == InstallStage::confirmed ||
           next == InstallStage::rolling_back;
  }
  if (current == InstallStage::rolling_back) {
    return next == InstallStage::rolled_back;
  }
  return false;
}

}  // namespace

Error InstallJournal::recover() noexcept {
  DurableRecord record{};
  const auto result = store_.load(record);
  if (result == Error::unavailable) {
    snapshot_ = {};
    return Error::none;
  }
  if (result != Error::none || !decode(record, snapshot_)) {
    return Error::storage;
  }
  return Error::none;
}

bool InstallJournal::active() const noexcept { return !terminal(snapshot_.stage); }

Error InstallJournal::persist(const InstallJournalSnapshot& value) noexcept {
  std::uint8_t payload[kPayloadBytes]{};
  encode(value, payload);
  std::uint32_t sequence = 0U;
  const auto result = store_.save(payload, sizeof(payload), snapshot_.sequence,
                                  sequence);
  if (result != Error::none) {
    return result;
  }
  snapshot_ = value;
  snapshot_.sequence = sequence;
  return Error::none;
}

Error InstallJournal::begin(std::string_view installation_id) noexcept {
  if (!valid_uuid(installation_id)) {
    return Error::invalid_argument;
  }
  if (active()) {
    return Error::busy;
  }
  InstallJournalSnapshot next{};
  std::memcpy(next.installation_id, installation_id.data(),
              installation_id.size());
  next.installation_id[installation_id.size()] = '\0';
  next.stage = InstallStage::prepared;
  return persist(next);
}

Error InstallJournal::transition(InstallStage next, Error error) noexcept {
  const bool failure = next == InstallStage::failed ||
                       next == InstallStage::cancelled;
  const bool records_error = next == InstallStage::failed ||
                             next == InstallStage::rolling_back;
  if ((!failure && !allowed(snapshot_.stage, next)) ||
      (failure && !active()) ||
      (records_error && error == Error::none) ||
      (!records_error && error != Error::none)) {
    return Error::invalid_argument;
  }
  auto value = snapshot_;
  value.stage = next;
  value.last_error = error;
  return persist(value);
}

}  // namespace hmi::update
