#include "hmi/update/durable_record.hpp"

#include <array>
#include <cstring>

namespace hmi::update {
namespace {

constexpr std::uint32_t kMagic = 0x31504455U;  // "UDP1" little endian
constexpr std::uint8_t kVersion = 1U;
constexpr std::size_t kHeaderBytes = 12U;
constexpr std::size_t kCrcBytes = 4U;

void put16(std::uint8_t* output, std::uint16_t value) noexcept {
  output[0] = static_cast<std::uint8_t>(value);
  output[1] = static_cast<std::uint8_t>(value >> 8U);
}

void put32(std::uint8_t* output, std::uint32_t value) noexcept {
  for (unsigned index = 0U; index < 4U; ++index) {
    output[index] = static_cast<std::uint8_t>(value >> (index * 8U));
  }
}

std::uint16_t get16(const std::uint8_t* input) noexcept {
  return static_cast<std::uint16_t>(input[0]) |
         static_cast<std::uint16_t>(input[1] << 8U);
}

std::uint32_t get32(const std::uint8_t* input) noexcept {
  std::uint32_t value = 0U;
  for (unsigned index = 0U; index < 4U; ++index) {
    value |= static_cast<std::uint32_t>(input[index]) << (index * 8U);
  }
  return value;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept {
  std::uint32_t value = 0xFFFFFFFFU;
  for (std::size_t index = 0U; index < size; ++index) {
    value ^= data[index];
    for (unsigned bit = 0U; bit < 8U; ++bit) {
      value = (value >> 1U) ^
              (0xEDB88320U & (0U - static_cast<std::uint32_t>(value & 1U)));
    }
  }
  return ~value;
}

std::size_t encode(const std::uint8_t* payload, std::size_t size,
                   std::uint32_t sequence,
                   std::uint8_t (&output)[kDurableRecordBytes]) noexcept {
  std::memset(output, 0, sizeof(output));
  put32(output, kMagic);
  output[4] = kVersion;
  put32(output + 6U, sequence);
  put16(output + 10U, static_cast<std::uint16_t>(size));
  std::memcpy(output + kHeaderBytes, payload, size);
  const auto used = kHeaderBytes + size;
  put32(output + used, crc32(output, used));
  return used + kCrcBytes;
}

bool decode(const std::uint8_t* input, std::size_t size,
            DurableRecord& output) noexcept {
  if (size < kHeaderBytes + kCrcBytes || get32(input) != kMagic ||
      input[4] != kVersion) {
    return false;
  }
  const auto payload_size = get16(input + 10U);
  const auto encoded_size = kHeaderBytes + payload_size + kCrcBytes;
  if (payload_size > kMaximumDurablePayloadBytes || size != encoded_size ||
      get32(input + kHeaderBytes + payload_size) !=
          crc32(input, kHeaderBytes + payload_size)) {
    return false;
  }
  DurableRecord parsed{};
  parsed.sequence = get32(input + 6U);
  parsed.size = payload_size;
  std::memcpy(parsed.payload, input + kHeaderBytes, payload_size);
  output = parsed;
  return true;
}

bool newer(std::uint32_t left, std::uint32_t right) noexcept {
  return static_cast<std::int32_t>(left - right) > 0;
}

}  // namespace

bool DurableRecordStore::read_slot(std::uint8_t slot,
                                   DurableRecord& output) const noexcept {
  if (port_.read == nullptr) {
    return false;
  }
  std::array<std::uint8_t, kDurableRecordBytes> encoded{};
  std::size_t used = 0U;
  return port_.read(port_.context, slot, encoded.data(), encoded.size(), used) ==
             Error::none &&
         decode(encoded.data(), used, output);
}

Error DurableRecordStore::load(DurableRecord& output) const noexcept {
  DurableRecord first{};
  DurableRecord second{};
  const bool has_first = read_slot(0U, first);
  const bool has_second = read_slot(1U, second);
  if (!has_first && !has_second) {
    return Error::unavailable;
  }
  output = !has_second || (has_first && newer(first.sequence, second.sequence))
               ? first
               : second;
  return Error::none;
}

Error DurableRecordStore::save(const std::uint8_t* payload, std::size_t size,
                               std::uint32_t current_sequence,
                               std::uint32_t& saved_sequence) const noexcept {
  if (payload == nullptr || size == 0U ||
      size > kMaximumDurablePayloadBytes || port_.write == nullptr ||
      port_.flush == nullptr || port_.read == nullptr) {
    return Error::invalid_argument;
  }
  const auto next = current_sequence + 1U;
  std::uint8_t encoded[kDurableRecordBytes]{};
  const auto used = encode(payload, size, next, encoded);
  const auto slot = static_cast<std::uint8_t>(next & 1U);
  auto result = port_.write(port_.context, slot, encoded, used);
  if (result != Error::none) {
    return result;
  }
  result = port_.flush(port_.context);
  if (result != Error::none) {
    return result;
  }
  DurableRecord verified{};
  if (!read_slot(slot, verified) || verified.sequence != next ||
      verified.size != size ||
      std::memcmp(verified.payload, payload, size) != 0) {
    return Error::storage;
  }
  saved_sequence = next;
  return Error::none;
}

}  // namespace hmi::update
