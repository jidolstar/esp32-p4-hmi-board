#include "hmi/update/asset_lifecycle.hpp"

#include <cstring>

namespace hmi::update {
namespace {

constexpr std::size_t kPayloadBytes = 111U;

bool empty(std::string_view value) noexcept {
  return value.empty() || value.front() == '\0';
}

void encode(const AssetGenerationSnapshot& value,
            std::uint8_t (&output)[kPayloadBytes]) noexcept {
  std::memcpy(output, value.active, 37U);
  std::memcpy(output + 37U, value.previous, 37U);
  std::memcpy(output + 74U, value.pending, 37U);
}

bool decode(const DurableRecord& record,
            AssetGenerationSnapshot& output) noexcept {
  if (record.size != kPayloadBytes || record.payload[36] != 0U ||
      record.payload[73] != 0U || record.payload[110] != 0U) {
    return false;
  }
  AssetGenerationSnapshot parsed{};
  std::memcpy(parsed.active, record.payload, 37U);
  std::memcpy(parsed.previous, record.payload + 37U, 37U);
  std::memcpy(parsed.pending, record.payload + 74U, 37U);
  parsed.sequence = record.sequence;
  if ((!empty(parsed.active) && !valid_uuid(parsed.active)) ||
      (!empty(parsed.previous) && !valid_uuid(parsed.previous)) ||
      (!empty(parsed.pending) && !valid_uuid(parsed.pending))) {
    return false;
  }
  output = parsed;
  return true;
}

bool copy_id(std::string_view value, char (&output)[37]) noexcept {
  if (!valid_uuid(value)) {
    return false;
  }
  std::memcpy(output, value.data(), value.size());
  output[value.size()] = '\0';
  return true;
}

}  // namespace

Error AssetLifecycle::recover() noexcept {
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

Error AssetLifecycle::persist(const AssetGenerationSnapshot& value) noexcept {
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

Error AssetLifecycle::adopt_active(std::string_view generation) noexcept {
  if (!empty(snapshot_.active) || !empty(snapshot_.pending)) {
    return Error::busy;
  }
  auto next = snapshot_;
  if (!copy_id(generation, next.active)) {
    return Error::invalid_argument;
  }
  return persist(next);
}

Error AssetLifecycle::prepare(std::string_view generation) noexcept {
  if (!empty(snapshot_.pending)) {
    return Error::busy;
  }
  if (generation == snapshot_.active || generation == snapshot_.previous) {
    return Error::binding;
  }
  auto next = snapshot_;
  if (!copy_id(generation, next.pending)) {
    return Error::invalid_argument;
  }
  return persist(next);
}

Error AssetLifecycle::activate_pending() noexcept {
  if (empty(snapshot_.pending)) {
    return Error::invalid_argument;
  }
  auto next = snapshot_;
  std::memcpy(next.previous, snapshot_.active, sizeof(next.previous));
  std::memcpy(next.active, snapshot_.pending, sizeof(next.active));
  std::memset(next.pending, 0, sizeof(next.pending));
  return persist(next);
}

Error AssetLifecycle::discard_pending() noexcept {
  if (empty(snapshot_.pending)) {
    return Error::invalid_argument;
  }
  auto next = snapshot_;
  std::memset(next.pending, 0, sizeof(next.pending));
  return persist(next);
}

Error AssetLifecycle::rollback_active() noexcept {
  if (empty(snapshot_.previous)) {
    return Error::unavailable;
  }
  auto next = snapshot_;
  char replacement[37]{};
  std::memcpy(replacement, next.active, sizeof(replacement));
  std::memcpy(next.active, next.previous, sizeof(next.active));
  std::memcpy(next.previous, replacement, sizeof(next.previous));
  std::memset(next.pending, 0, sizeof(next.pending));
  return persist(next);
}

}  // namespace hmi::update
