#include "camera/gallery_jpeg.hpp"

#include <memory>

#include <driver/jpeg_decode.h>
#include <driver/jpeg_encode.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "log.hpp"

namespace hmi::platform::hmi_host::camera {
namespace {
using namespace ::hmi::camera;

constexpr std::uint32_t kWidthLimit = 1288U;
constexpr std::uint32_t kHeightLimit = 728U;
constexpr int kCodecTimeoutMs = 800;
constexpr std::uint32_t align_mcu(std::uint32_t dimension) noexcept {
  return (dimension + 15U) & ~15U;
}

struct PsrDeleter {
  void operator()(std::uint8_t* memory) const noexcept { heap_caps_free(memory); }
};
using PsrBuffer = std::unique_ptr<std::uint8_t, PsrDeleter>;

std::uint32_t read_be16(const std::uint8_t* data) noexcept {
  return (static_cast<std::uint32_t>(data[0]) << 8U) | data[1];
}

/** 카메라가 생성하는 baseline 4:2:0만 받아 DMA에 넘길 크기와 샘플링 값을 먼저 제한한다. */
bool read_dimensions(const std::uint8_t* jpeg, std::size_t bytes,
                     std::uint32_t& width, std::uint32_t& height) noexcept {
  std::size_t cursor = 2U;
  while (cursor + 4U <= bytes && jpeg[cursor] == 0xffU) {
    const auto marker = jpeg[cursor + 1U];
    const auto length = read_be16(jpeg + cursor + 2U);
    cursor += 2U;
    if (length < 2U || length > bytes - cursor) {
      return false;
    }
    if (marker == 0xdaU) {
      return width != 0U && height != 0U && length == 12U && jpeg[cursor + 2U] == 3U &&
             cursor + length < bytes;
    }
    if (marker == 0xc0U) {
      if (width != 0U || length != 17U || jpeg[cursor + 2U] != 8U || jpeg[cursor + 7U] != 3U ||
          jpeg[cursor + 9U] != 0x22U || jpeg[cursor + 12U] != 0x11U || jpeg[cursor + 15U] != 0x11U) {
        return false;
      }
      height = read_be16(jpeg + cursor + 3U);
      width = read_be16(jpeg + cursor + 5U);
      if (width == 0U || width > kWidthLimit || height == 0U || height > kHeightLimit ||
          (width * height) % 8U != 0U) {
        return false;
      }
    } else if (marker != 0xc4U && marker != 0xdbU && marker != 0xddU && marker != 0xfeU &&
               (marker < 0xe0U || marker > 0xefU)) {
      return false;
    }
    cursor += length;
  }
  return false;
}

void free_decoded(void* context, std::uint32_t) noexcept { heap_caps_free(context); }
}  // namespace

using namespace ::hmi::camera;

GalleryError GalleryJpeg::save(CameraFrameLease frame, PhotoId& saved_id) noexcept {
  saved_id = 0U;
  const auto& view = frame.view();
  const auto bytes = static_cast<std::uint32_t>(view.width) * view.height * 2U;
  if (!frame || !view.pixels || view.width == 0U || view.width > kWidthLimit ||
      view.height == 0U || view.height > kHeightLimit ||
      view.stride_bytes != view.width * 2U || view.bytes < bytes) {
    return GalleryError::write_failed;
  }

  const jpeg_encode_memory_alloc_cfg_t memory{JPEG_ENC_ALLOC_OUTPUT_BUFFER};
  std::size_t capacity = 0U;
  const PsrBuffer compressed(static_cast<std::uint8_t*>(
      jpeg_alloc_encoder_mem(GallerySdFiles::kMaxJpegBytes, &memory, &capacity)));
  jpeg_encoder_handle_t encoder = nullptr;
  const jpeg_encode_engine_cfg_t engine{0, kCodecTimeoutMs};
  if (!compressed || jpeg_new_encoder_engine(&engine, &encoder) != ESP_OK) {
    return GalleryError::write_failed;
  }
  const jpeg_encode_cfg_t encode{view.height, view.width, JPEG_ENCODE_IN_FORMAT_RGB565,
                                 JPEG_DOWN_SAMPLING_YUV420, 80U};
  const auto encode_start = esp_timer_get_time();
  std::uint32_t written = 0U;
  const auto result = jpeg_encoder_process(encoder, &encode, view.pixels, bytes,
                                           compressed.get(), capacity, &written);
  jpeg_del_encoder_engine(encoder);
  // JPEG 출력이 완성되면 SD 작업 전에 CSI 프레임을 반환한다.
  frame.reset();
  if (result != ESP_OK) return GalleryError::write_failed;
  const auto sd_start = esp_timer_get_time();
  const auto status = files_.save_jpeg(compressed.get(), written, saved_id);
  HMI_LOG_INFO("[camera] save bytes=%u encode_us=%u sd_us=%u status=%u\r\n",
               static_cast<unsigned>(written), static_cast<unsigned>(sd_start - encode_start),
               static_cast<unsigned>(esp_timer_get_time() - sd_start), static_cast<unsigned>(status));
  return status;
}

GalleryError GalleryJpeg::open(PhotoId id, CameraFrameLease& image) noexcept {
  image.reset();
  const jpeg_decode_memory_alloc_cfg_t input{JPEG_DEC_ALLOC_INPUT_BUFFER};
  std::size_t input_capacity = 0U;
  const PsrBuffer compressed(static_cast<std::uint8_t*>(
      jpeg_alloc_decoder_mem(GallerySdFiles::kMaxJpegBytes, &input, &input_capacity)));
  if (!compressed) {
    return GalleryError::lease_busy;
  }
  std::size_t bytes = 0U;
  const auto file_result = files_.read_jpeg(id, compressed.get(), input_capacity, bytes);
  if (file_result != GalleryError::none) {
    return file_result;
  }
  std::uint32_t width = 0U;
  std::uint32_t height = 0U;
  if (!read_dimensions(compressed.get(), bytes, width, height)) {
    return GalleryError::invalid_file;
  }

  const std::uint32_t stride = align_mcu(width) * 2U;
  const std::uint32_t decoded_bytes = stride * align_mcu(height);
  const jpeg_decode_memory_alloc_cfg_t output{JPEG_DEC_ALLOC_OUTPUT_BUFFER};
  std::size_t output_capacity = 0U;
  PsrBuffer decoded(static_cast<std::uint8_t*>(
      jpeg_alloc_decoder_mem(decoded_bytes, &output, &output_capacity)));
  jpeg_decoder_handle_t decoder = nullptr;
  const jpeg_decode_engine_cfg_t engine{0, kCodecTimeoutMs};
  if (!decoded || jpeg_new_decoder_engine(&engine, &decoder) != ESP_OK) {
    return GalleryError::lease_busy;
  }
  const jpeg_decode_cfg_t decode{JPEG_DECODE_OUT_FORMAT_RGB565,
                                 JPEG_DEC_RGB_ELEMENT_ORDER_BGR, JPEG_YUV_RGB_CONV_STD_BT601};
  std::uint32_t actual = 0U;
  const auto result = jpeg_decoder_process(decoder, &decode, compressed.get(), bytes,
                                           decoded.get(), output_capacity, &actual);
  jpeg_del_decoder_engine(decoder);
  if (result != ESP_OK || actual != decoded_bytes) {
    return GalleryError::invalid_file;
  }
  auto* pixels = decoded.release();
  image = CameraFrameLease({pixels, actual, 0U, static_cast<std::uint16_t>(width),
                            static_cast<std::uint16_t>(height), stride}, pixels, free_decoded);
  return GalleryError::none;
}

}  // namespace hmi::platform::hmi_host::camera
