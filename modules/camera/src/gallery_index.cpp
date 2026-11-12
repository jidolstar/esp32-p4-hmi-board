#include "hmi/camera/gallery_index.hpp"

#include <algorithm>
#include <charconv>
#include <system_error>

namespace hmi::camera {

namespace {
constexpr std::string_view kPrefix = "IMG_";
constexpr std::string_view kExtension = ".jpg";
}  // namespace

PhotoId parse_photo_filename(std::string_view filename) noexcept {
  if (filename.size() <= kPrefix.size() + kExtension.size() ||
      filename.size() >= PhotoFilename{}.size() ||
      filename.substr(0U, kPrefix.size()) != kPrefix ||
      filename.substr(filename.size() - kExtension.size()) != kExtension) {
    return 0U;
  }
  const char* first = filename.data() + kPrefix.size();
  const char* last = filename.data() + filename.size() - kExtension.size();
  if (*first < '1' || *first > '9') {
    return 0U;
  }
  PhotoId id = 0U;
  const auto parsed = std::from_chars(first, last, id);
  return parsed.ec == std::errc{} && parsed.ptr == last ? id : 0U;
}

PhotoFilename photo_filename(PhotoId id) noexcept {
  PhotoFilename result{};
  if (id != 0U) {
    char* digits = std::copy(kPrefix.begin(), kPrefix.end(), result.data());
    const auto formatted = std::to_chars(digits, result.end() - kExtension.size() - 1U, id);
    std::copy(kExtension.begin(), kExtension.end(), formatted.ptr);
  }
  return result;
}

bool GalleryIndex::add(std::string_view filename, PhotoMetadata metadata) noexcept {
  const PhotoId id = parse_photo_filename(filename);
  if (id == 0U) {
    return false;
  }

  const auto first = items_.begin();
  const auto end = first + count_;
  const auto position = std::lower_bound(first, end, id, [](const auto& item, PhotoId key) {
    return item.id > key;
  });
  if (position == items_.end() || (position != end && position->id == id)) {
    return false;
  }

  // 가득 찬 목록의 마지막 항목만 버리고 나머지는 최신순을 유지한다.
  const auto last = count_ < items_.size() ? end : end - 1;
  std::move_backward(position, last, last + 1);
  metadata.id = id;
  *position = metadata;
  count_ = std::min(count_ + 1U, items_.size());
  return true;
}

bool GalleryIndex::contains(PhotoId id) const noexcept {
  const auto end = items_.begin() + count_;
  return std::binary_search(items_.begin(), end, PhotoMetadata{id},
                            [](const auto& left, const auto& right) { return left.id > right.id; });
}

void GalleryIndex::read_page(std::size_t page_index, GalleryPage& output) const noexcept {
  output = {};
  if (page_index >= (count_ + kGalleryPageCapacity - 1U) / kGalleryPageCapacity) {
    return;
  }
  const std::size_t offset = page_index * kGalleryPageCapacity;
  output.count = std::min(count_ - offset, kGalleryPageCapacity);
  std::copy_n(items_.begin() + offset, output.count, output.items.begin());
}

}  // namespace hmi::camera
