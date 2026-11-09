#include "internal/wav_reader.hpp"

#include <cstring>

namespace hmi::boards::jc1060p470c::internal {
namespace {

std::uint16_t le16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(data[0]) |
         static_cast<std::uint16_t>(data[1] << 8U);
}

std::uint32_t le32(const std::uint8_t* data) {
  return static_cast<std::uint32_t>(data[0]) |
         (static_cast<std::uint32_t>(data[1]) << 8U) |
         (static_cast<std::uint32_t>(data[2]) << 16U) |
         (static_cast<std::uint32_t>(data[3]) << 24U);
}

bool has_tag(const std::uint8_t* data, const char* value) {
  return std::memcmp(data, value, 4U) == 0;
}

std::int32_t pcm_sample(const std::uint8_t* data, std::uint16_t bits) {
  if (bits == 8U) {
    return (static_cast<std::int32_t>(data[0]) - 128) * 256;
  }
  if (bits == 16U) {
    return static_cast<std::int16_t>(le16(data));
  }
  const std::uint32_t raw = static_cast<std::uint32_t>(data[0]) |
                            (static_cast<std::uint32_t>(data[1]) << 8U) |
                            (static_cast<std::uint32_t>(data[2]) << 16U);
  const std::int32_t signed_value =
      (raw & 0x800000U) != 0U ? static_cast<std::int32_t>(raw) - 0x1000000
                              : static_cast<std::int32_t>(raw);
  const std::int32_t rounded =
      (signed_value + (signed_value >= 0 ? 128 : -128)) / 256;
  return rounded > 32767 ? 32767 : (rounded < -32768 ? -32768 : rounded);
}

}  // namespace

bool wav_sample_rate_supported(std::uint32_t rate) noexcept {
  return rate == 8000U || rate == 16000U || rate == 22050U ||
         rate == 32000U || rate == 44100U || rate == 48000U;
}

std::size_t wav_frame_bytes(const WavInfo& info) noexcept {
  return static_cast<std::size_t>(info.channels) *
         (info.bits_per_sample / 8U);
}

WavResult parse_wav(WavReadAt read, void* context, std::uint64_t file_size,
                    WavInfo& out) {
  out = {};
  if (read == nullptr || file_size < 12U) {
    return WavResult::truncated;
  }
  std::uint8_t header[24]{};
  if (!read(context, 0U, header, 12U)) {
    return WavResult::truncated;
  }
  if (!has_tag(header, "RIFF") || !has_tag(header + 8U, "WAVE")) {
    return WavResult::invalid;
  }
  const std::uint64_t riff_end = static_cast<std::uint64_t>(le32(header + 4U)) + 8U;
  if (riff_end < 12U || riff_end > file_size) {
    return WavResult::truncated;
  }

  bool have_format = false;
  bool have_data = false;
  for (std::uint64_t at = 12U; at < riff_end;) {
    if (riff_end - at < 8U || !read(context, at, header, 8U)) {
      return WavResult::truncated;
    }
    const std::uint32_t length = le32(header + 4U);
    const std::uint64_t body = at + 8U;
    const std::uint64_t end = body + static_cast<std::uint64_t>(length);
    const std::uint64_t padded = end + (length & 1U);
    if (end > riff_end || padded > riff_end) {
      return WavResult::truncated;
    }
    if (has_tag(header, "fmt ") && !have_format) {
      if (length < 16U || !read(context, body, header, 16U)) {
        return WavResult::truncated;
      }
      const std::uint16_t encoding = le16(header);
      out.channels = le16(header + 2U);
      out.sample_rate = le32(header + 4U);
      const std::uint32_t byte_rate = le32(header + 8U);
      const std::uint16_t alignment = le16(header + 12U);
      out.bits_per_sample = le16(header + 14U);
      if (encoding != 1U || (out.channels != 1U && out.channels != 2U) ||
          !wav_sample_rate_supported(out.sample_rate) ||
          (out.bits_per_sample != 8U && out.bits_per_sample != 16U &&
           out.bits_per_sample != 24U) ||
          alignment != wav_frame_bytes(out) ||
          static_cast<std::uint64_t>(byte_rate) !=
              static_cast<std::uint64_t>(out.sample_rate) * alignment) {
        return WavResult::unsupported;
      }
      have_format = true;
    } else if (has_tag(header, "data")) {
      if (have_data) {
        return WavResult::unsupported;
      }
      out.data_offset = body;
      out.data_bytes = length;
      have_data = true;
    }
    at = padded;
  }
  const std::size_t frame_bytes = wav_frame_bytes(out);
  if (!have_format || !have_data || frame_bytes == 0U ||
      out.data_bytes == 0U || out.data_bytes % frame_bytes != 0U) {
    return WavResult::invalid;
  }
  return WavResult::ok;
}

std::size_t convert_pcm_to_stereo_slots(const std::uint8_t* source,
                                        std::size_t source_bytes,
                                        const WavInfo& info,
                                        std::int16_t* target,
                                        std::size_t target_samples) {
  const std::size_t sample_bytes = info.bits_per_sample / 8U;
  const std::size_t frame_bytes = wav_frame_bytes(info);
  if (source == nullptr || target == nullptr || sample_bytes == 0U ||
      frame_bytes == 0U || source_bytes % frame_bytes != 0U) {
    return 0U;
  }
  const std::size_t frames = source_bytes / frame_bytes;
  if (frames == 0U || frames > target_samples / 2U) {
    return 0U;
  }
  for (std::size_t index = 0U; index < frames; ++index) {
    const std::uint8_t* frame = source + index * frame_bytes;
    const std::int32_t left = pcm_sample(frame, info.bits_per_sample);
    const std::int32_t right = info.channels == 2U
                                   ? pcm_sample(frame + sample_bytes,
                                                info.bits_per_sample)
                                   : left;
    const auto mono = static_cast<std::int16_t>((left + right) / 2);
    target[2U * index] = mono;
    target[2U * index + 1U] = mono;
  }
  return frames * 2U;
}

}  // namespace hmi::boards::jc1060p470c::internal
