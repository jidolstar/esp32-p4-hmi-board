#pragma once

#include <cstddef>
#include <cstdint>

namespace hmi::boards::jc1060p470c::internal {

struct WavInfo {
  std::uint64_t data_offset = 0U;
  std::uint32_t data_bytes = 0U;
  std::uint16_t channels = 0U;
  std::uint16_t bits_per_sample = 0U;
  std::uint32_t sample_rate = 0U;
};

enum class WavResult : std::uint8_t { ok, invalid, unsupported, truncated };
using WavReadAt = bool (*)(void*, std::uint64_t, std::uint8_t*, std::size_t);

/** WAV 전체를 적재하지 않고 PCM mono/stereo header와 chunk 범위만 검사한다. */
WavResult parse_wav(WavReadAt read, void* context, std::uint64_t file_size,
                    WavInfo& out);
bool wav_sample_rate_supported(std::uint32_t rate) noexcept;
std::size_t wav_frame_bytes(const WavInfo& info) noexcept;
std::uint32_t wav_milliseconds_for_bytes(std::uint32_t bytes,
                                         const WavInfo& info) noexcept;

/** PCM 8/16/24-bit mono/stereo를 ES8311용 signed 16-bit stereo slot으로 바꾼다. */
std::size_t convert_pcm_to_stereo_slots(const std::uint8_t* source,
                                        std::size_t source_bytes,
                                        const WavInfo& info,
                                        std::int16_t* target,
                                        std::size_t target_samples);

}  // namespace hmi::boards::jc1060p470c::internal
