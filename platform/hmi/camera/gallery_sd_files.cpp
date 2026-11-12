#include "camera/gallery_sd_files.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace hmi::platform::hmi_host::camera {
namespace {
using ::hmi::camera::GalleryError;

constexpr std::string_view kTemporarySuffix = ".tmp";

bool has_jpeg_markers(const std::uint8_t* bytes, std::size_t size) noexcept {
  return bytes && size >= 4U && bytes[0] == 0xffU && bytes[1] == 0xd8U &&
         bytes[size - 2U] == 0xffU && bytes[size - 1U] == 0xd9U;
}
}  // namespace

using namespace ::hmi::camera;

GallerySdFiles::GallerySdFiles(::hmi::boards::jc1060p470c::Storage& storage,
                               const char* profile_root) noexcept
    : storage_(storage) {
  if (!profile_root || profile_root[0] != '/' || profile_root[1] == '\0') {
    return;
  }
  const int count = std::snprintf(root_.data(), root_.size(), "%s/camera-gallery", profile_root);
  if (count < 0 || static_cast<std::size_t>(count) >= root_.size()) {
    root_[0] = '\0';
  }
}

bool GallerySdFiles::photo_path(PhotoId id, Path& output, bool temporary) const noexcept {
  if (id == 0U || root_.front() == '\0') {
    return false;
  }
  const auto name = photo_filename(id);
  const int count = std::snprintf(output.data(), output.size(), "%s/%s%s", root_.data(),
                                  name.data(), temporary ? kTemporarySuffix.data() : "");
  return count > 0 && static_cast<std::size_t>(count) < output.size();
}

GalleryError GallerySdFiles::lease_error() const noexcept {
  return storage_.ready() ? GalleryError::lease_busy : GalleryError::not_ready;
}

void GallerySdFiles::index_file(void* context, std::string_view name, std::uint64_t bytes) noexcept {
  auto& self = *static_cast<GallerySdFiles*>(context);
  if (name.size() > kTemporarySuffix.size() &&
      name.substr(name.size() - kTemporarySuffix.size()) == kTemporarySuffix) {
    // 중단된 저장의 이름은 재사용하지 않으며 완성 사진 목록에서는 건너뛴다.
    const auto id = parse_photo_filename(name.substr(0U, name.size() - kTemporarySuffix.size()));
    self.last_id_ = std::max(self.last_id_, id);
    if (id != 0U && self.recovery_count_ < self.recovery_ids_.size()) {
      self.recovery_ids_[self.recovery_count_++] = id;
    }
    return;
  }
  const auto id = parse_photo_filename(name);
  self.last_id_ = std::max(self.last_id_, id);
  if (id != 0U && bytes <= std::numeric_limits<std::uint32_t>::max()) {
    self.index_.add(name, {id, 0U, static_cast<std::uint32_t>(bytes), 0U, 0U});
  }
}

bool GallerySdFiles::scan_directory() noexcept {
  using ::hmi::boards::jc1060p470c::FileVisitResult;
  index_.clear();
  recovery_count_ = 0U;
  if (root_.front() == '\0') {
    return false;
  }
  const auto result = storage_.visit_regular_files(root_.data(), this, index_file);
  if (result == FileVisitResult::missing) {
    return true;
  }
  if (result != FileVisitResult::success) {
    return false;
  }
  // FAT 디렉터리 순회 중에는 항목을 삭제하지 않는다. 닫은 뒤 확인된 임시 파일만 정리한다.
  Path path{};
  for (std::size_t i = 0U; i < recovery_count_; ++i) {
    if (photo_path(recovery_ids_[i], path, true)) {
      storage_.remove_regular_file(path.data());
    }
  }
  return true;
}

GalleryError GallerySdFiles::scan(std::size_t page_index, GalleryCompletion& result) noexcept {
  const auto lease = storage_.acquire_exclusive_lease();
  if (!lease) {
    return lease_error();
  }
  if (!scan_directory()) {
    return GalleryError::invalid_file;
  }
  result.total = index_.size();
  const std::size_t last_page = result.total == 0U ? 0U : (result.total - 1U) / kGalleryPageCapacity;
  result.page_index = std::min(page_index, last_page);
  index_.read_page(result.page_index, result.page);
  return GalleryError::none;
}

GalleryError GallerySdFiles::save_jpeg(const std::uint8_t* jpeg, std::size_t size, PhotoId& saved_id) noexcept {
  saved_id = 0U;
  if (size > kMaxJpegBytes || !has_jpeg_markers(jpeg, size)) {
    return GalleryError::write_failed;
  }
  // 임시 파일 생성부터 최종 rename까지 같은 lease로 raw owner 전환을 차단한다.
  const auto lease = storage_.acquire_exclusive_lease();
  if (!lease) {
    return lease_error();
  }
  if (root_.front() == '\0' || !storage_.mkdirs(root_.data()) || !scan_directory()) {
    return GalleryError::write_failed;
  }
  if (storage_.free_bytes() < size || last_id_ == std::numeric_limits<PhotoId>::max()) {
    return GalleryError::full;
  }
  const PhotoId id = ++last_id_;
  Path temp{};
  Path final{};
  std::uint64_t actual_size = 0U;
  // FAT rename은 대상 이름이 이미 존재하면 실패한다. 부분 파일은 .tmp로 남긴다.
  if (!photo_path(id, temp, true) || !photo_path(id, final) || storage_.exists(final.data()) ||
      !storage_.write_new_file(temp.data(), jpeg, size) ||
      !storage_.file_size(temp.data(), actual_size) || actual_size != size ||
      !storage_.rename_path(temp.data(), final.data())) {
    return GalleryError::write_failed;
  }
  saved_id = id;
  return GalleryError::none;
}

GalleryError GallerySdFiles::read_jpeg(PhotoId id, std::uint8_t* buffer, std::size_t capacity,
                                       std::size_t& bytes_read) noexcept {
  bytes_read = 0U;
  const auto lease = storage_.acquire_exclusive_lease();
  if (!lease) {
    return lease_error();
  }
  Path path{};
  std::uint64_t size = 0U;
  std::size_t actual_size = 0U;
  if (!buffer || !photo_path(id, path) || !storage_.file_size(path.data(), size) ||
      size < 4U || size > kMaxJpegBytes || size > capacity ||
      !storage_.read_file_at(path.data(), 0U, buffer, static_cast<std::size_t>(size), actual_size) ||
      actual_size != size || !has_jpeg_markers(buffer, actual_size)) {
    return GalleryError::invalid_file;
  }
  bytes_read = actual_size;
  return GalleryError::none;
}

GalleryError GallerySdFiles::erase(PhotoId id) noexcept {
  const auto lease = storage_.acquire_exclusive_lease();
  if (!lease) {
    return lease_error();
  }
  Path path{};
  return photo_path(id, path) && storage_.remove_regular_file(path.data())
             ? GalleryError::none : GalleryError::delete_failed;
}

}  // namespace hmi::platform::hmi_host::camera
