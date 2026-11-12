#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hmi::camera {

using PhotoId = std::uint64_t;
inline constexpr std::size_t kGalleryIndexCapacity = 60U;
inline constexpr std::size_t kGalleryPageCapacity = 8U;
using PhotoFilename = std::array<char, 29U>;  // IMG_, uint64 20자리, .jpg, 종결 문자

/** JPEG 파일의 이름만 해석한다. 실패하면 0을 반환한다. */
[[nodiscard]] PhotoId parse_photo_filename(std::string_view filename) noexcept;

/** 유효한 사진 ID의 canonical 파일명을 만들고, 실패하면 빈 이름을 반환한다. */
[[nodiscard]] PhotoFilename photo_filename(PhotoId id) noexcept;

struct PhotoMetadata {
  PhotoId id = 0U;
  std::uint64_t timestamp = 0U;
  std::uint32_t bytes = 0U;
  std::uint16_t width = 0U;
  std::uint16_t height = 0U;
};

struct GalleryPage {
  std::array<PhotoMetadata, kGalleryPageCapacity> items{};
  std::size_t count = 0U;
};

/** 저장소가 스캔한 사진 중 최신 ID 60개만 보관한다. 호출자는 이전 인덱스를 비우고 재스캔한다. */
class GalleryIndex {
 public:
  /** 임시 파일, 유효하지 않은 이름, 중복 ID는 목록에 넣지 않는다. */
  bool add(std::string_view filename, PhotoMetadata metadata) noexcept;
  [[nodiscard]] bool contains(PhotoId id) const noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return count_; }
  void clear() noexcept { count_ = 0U; }
  void read_page(std::size_t page_index, GalleryPage& output) const noexcept;

 private:
  std::array<PhotoMetadata, kGalleryIndexCapacity> items_{};
  std::size_t count_ = 0U;
};

static_assert(sizeof(PhotoMetadata) <= 24U, "photo metadata must remain bounded");
static_assert(sizeof(GalleryPage) <= 208U, "gallery page must remain bounded");
static_assert(sizeof(GalleryIndex) <= 1456U, "gallery index must remain bounded");

}  // namespace hmi::camera
