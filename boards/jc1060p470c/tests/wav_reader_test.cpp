#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "internal/wav_reader.hpp"

namespace {

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t at,
               std::uint16_t value) {
  bytes[at] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[at + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t at,
               std::uint32_t value) {
  bytes[at] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[at + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  bytes[at + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  bytes[at + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

bool read_at(void* context, std::uint64_t offset, std::uint8_t* output,
             std::size_t size) {
  auto& bytes = *static_cast<std::vector<std::uint8_t>*>(context);
  if (offset > bytes.size() || size > bytes.size() - offset) {
    return false;
  }
  std::memcpy(output, bytes.data() + offset, size);
  return true;
}

std::vector<std::uint8_t> make_wav(std::uint32_t rate) {
  constexpr std::uint32_t kDataBytes = 96000U;
  std::vector<std::uint8_t> bytes(44U + kDataBytes, 0U);
  std::memcpy(bytes.data(), "RIFF", 4U);
  write_u32(bytes, 4U, static_cast<std::uint32_t>(bytes.size() - 8U));
  std::memcpy(bytes.data() + 8U, "WAVEfmt ", 8U);
  write_u32(bytes, 16U, 16U);
  write_u16(bytes, 20U, 1U);
  write_u16(bytes, 22U, 1U);
  write_u32(bytes, 24U, rate);
  write_u32(bytes, 28U, rate * 2U);
  write_u16(bytes, 32U, 2U);
  write_u16(bytes, 34U, 16U);
  std::memcpy(bytes.data() + 36U, "data", 4U);
  write_u32(bytes, 40U, kDataBytes);
  return bytes;
}

}  // namespace

int main() {
  using namespace hmi::boards::jc1060p470c::internal;

  auto wav = make_wav(48000U);
  WavInfo info;
  assert(parse_wav(read_at, &wav, wav.size(), info) == WavResult::ok);
  assert(info.data_bytes == 96000U);
  assert(wav_milliseconds_for_bytes(info.data_bytes, info) == 1000U);
  assert(wav_milliseconds_for_bytes(info.data_bytes / 4U, info) == 250U);

  auto unsupported = make_wav(24000U);
  assert(parse_wav(read_at, &unsupported, unsupported.size(), info) ==
         WavResult::unsupported);
  return 0;
}

