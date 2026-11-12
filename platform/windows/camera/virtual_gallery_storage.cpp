#include "camera/virtual_gallery_storage.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace hmi::platform::windows::camera {
namespace {
using namespace hmi::camera;

constexpr std::string_view kTemporarySuffix = ".tmp";
constexpr auto kWidth = VirtualCameraPort::kWidth;
constexpr auto kHeight = VirtualCameraPort::kHeight;
constexpr std::size_t kRowBytes = kWidth * 2U;
}  // namespace

bool VirtualGalleryStorage::request(Work work, std::uint32_t token) noexcept {
  if (token == 0U || busy()) {
    return false;
  }
  work_ = work;
  request_token_ = token;
  return true;
}

bool VirtualGalleryStorage::request_scan(std::uint32_t token, std::size_t page_index) noexcept {
  if (!request(Work::scan, token)) {
    return false;
  }
  page_index_ = page_index;
  return true;
}

bool VirtualGalleryStorage::request_save(std::uint32_t token, CameraFrameLease frame) noexcept {
  if (!frame || !request(Work::save, token)) {
    return false;
  }
  frame_ = std::move(frame);
  return true;
}

bool VirtualGalleryStorage::request_open(std::uint32_t token, PhotoId id) noexcept {
  if (id == 0U || viewer_borrowed_ || !request(Work::open, token)) {
    return false;
  }
  id_ = id;
  return true;
}

bool VirtualGalleryStorage::request_delete(std::uint32_t token, PhotoId id) noexcept {
  if (id == 0U || viewer_borrowed_ || !request(Work::erase, token)) {
    return false;
  }
  id_ = id;
  return true;
}

bool VirtualGalleryStorage::busy() const noexcept {
  return work_ != Work::none || completion_.token != 0U;
}

VirtualGalleryStorage::File* VirtualGalleryStorage::find(std::string_view filename) noexcept {
  const auto position = std::find_if(files_.begin(), files_.end(), [filename](const File& file) {
    return filename == file.name.data();
  });
  return position == files_.end() ? nullptr : &*position;
}

bool VirtualGalleryStorage::exists(std::string_view filename) const noexcept {
  return !filename.empty() && std::any_of(files_.begin(), files_.end(), [filename](const File& file) {
    return filename == file.name.data();
  });
}

bool VirtualGalleryStorage::seed(std::string_view filename, FileKind kind) noexcept {
  if (filename.empty() || filename.size() >= Filename{}.size() ||
      filename.find('\0') != std::string_view::npos || find(filename)) {
    return false;
  }
  auto* file = find("");
  if (!file) {
    return false;
  }
  std::copy(filename.begin(), filename.end(), file->name.begin());
  file->kind = kind;
  file->pixels.fill(0x42U);
  file->bytes = kind == FileKind::corrupt ? 1U : static_cast<std::uint32_t>(file->pixels.size());
  last_id_ = std::max(last_id_, parse_photo_filename(filename));
  return true;
}

GalleryError VirtualGalleryStorage::scan() noexcept {
  index_.clear();
  for (auto& file : files_) {
    if (file.kind == FileKind::non_regular || file.name.front() == '\0') {
      continue;
    }
    const std::string_view name(file.name.data());
    if (name.size() > kTemporarySuffix.size() &&
        name.substr(name.size() - kTemporarySuffix.size()) == kTemporarySuffix) {
      const auto id = parse_photo_filename(name.substr(0U, name.size() - kTemporarySuffix.size()));
      if (id != 0U) {
        last_id_ = std::max(last_id_, id);
        file = {};
      }
      continue;
    }
    index_.add(name, {0U, 0U, file.bytes, kWidth, kHeight});
  }
  completion_.total = index_.size();
  const auto last_page = completion_.total == 0U ? 0U : (completion_.total - 1U) / kGalleryPageCapacity;
  completion_.page_index = std::min(page_index_, last_page);
  index_.read_page(completion_.page_index, completion_.page);
  return GalleryError::none;
}

GalleryError VirtualGalleryStorage::stage_write() noexcept {
  const auto& view = frame_.view();
  if (view.width != kWidth || view.height != kHeight || !view.pixels ||
      view.stride_bytes < kRowBytes || view.bytes < kRowBytes ||
      view.stride_bytes > (view.bytes - kRowBytes) / (kHeight - 1U)) {
    return GalleryError::write_failed;
  }
  auto* file = find("");
  if (!file) {
    return GalleryError::full;
  }

  Filename temporary{};
  do {
    if (last_id_ == std::numeric_limits<PhotoId>::max()) {
      return GalleryError::full;
    }
    id_ = ++last_id_;
    const auto final = photo_filename(id_);
    const std::string_view name(final.data());
    auto suffix = std::copy(name.begin(), name.end(), temporary.begin());
    std::copy(kTemporarySuffix.begin(), kTemporarySuffix.end(), suffix);
  } while (find(temporary.data()));

  // 원본의 stride를 따르되 padding은 fixture 사진에 포함하지 않는다.
  for (std::size_t row = 0U; row < kHeight; ++row) {
    std::copy_n(view.pixels + row * view.stride_bytes, kRowBytes, file->pixels.begin() + row * kRowBytes);
  }
  file->bytes = next_error_ == GalleryError::write_failed
                    ? static_cast<std::uint32_t>(file->pixels.size() / 2U)
                    : static_cast<std::uint32_t>(file->pixels.size());
  next_error_ = GalleryError::none;
  file->name = temporary;
  staged_ = file;
  frame_.reset();
  return GalleryError::none;
}

GalleryError VirtualGalleryStorage::publish_write() noexcept {
  const auto final = photo_filename(id_);
  if (staged_->bytes != staged_->pixels.size() || find(final.data())) {
    return GalleryError::write_failed;
  }
  // 단일 이름 교체로 commit을 재현한다. 충돌 시 기존 사진과 임시 파일을 그대로 둔다.
  staged_->name = {};
  std::copy(final.begin(), final.end(), staged_->name.begin());
  completion_.saved_id = id_;
  return GalleryError::none;
}

GalleryError VirtualGalleryStorage::open() noexcept {
  const auto name = photo_filename(id_);
  const auto* file = find(name.data());
  if (!file || file->kind != FileKind::photo || file->bytes != file->pixels.size()) {
    return GalleryError::invalid_file;
  }
  if (viewer_borrowed_) {
    return GalleryError::lease_busy;
  }
  viewer_pixels_ = file->pixels;
  viewer_borrowed_ = true;
  completion_.image = {{viewer_pixels_.data(), viewer_pixels_.size(), 0U, kWidth, kHeight, kRowBytes},
                       this, release_viewer};
  return GalleryError::none;
}

GalleryError VirtualGalleryStorage::erase() noexcept {
  const auto name = photo_filename(id_);
  auto* file = find(name.data());
  if (!file || file->kind == FileKind::non_regular) {
    return GalleryError::delete_failed;
  }
  if (viewer_borrowed_) {
    return GalleryError::lease_busy;
  }
  *file = {};
  return GalleryError::none;
}

void VirtualGalleryStorage::release_viewer(void* context, std::uint32_t) noexcept {
  static_cast<VirtualGalleryStorage*>(context)->viewer_borrowed_ = false;
}

void VirtualGalleryStorage::finish(GalleryError error) noexcept {
  completion_.token = std::exchange(request_token_, 0U);
  completion_.error = error;
  work_ = Work::none;
  staged_ = nullptr;
  frame_.reset();
}

void VirtualGalleryStorage::advance() noexcept {
  if (work_ == Work::none || completion_.token != 0U) {
    return;
  }
  // write_failed는 임시 파일을 쓴 다음 commit 전에 주입한다.
  if (work_ == Work::save && !staged_ &&
      (next_error_ == GalleryError::none || next_error_ == GalleryError::write_failed)) {
    const auto error = stage_write();
    if (error != GalleryError::none) {
      next_error_ = GalleryError::none;
      finish(error);
    }
    return;
  }
  const auto injected = std::exchange(next_error_, GalleryError::none);
  if (injected != GalleryError::none) {
    finish(injected);
    return;
  }
  switch (work_) {
    case Work::scan: finish(scan()); break;
    case Work::save: finish(publish_write()); break;
    case Work::open: finish(open()); break;
    case Work::erase: finish(erase()); break;
    case Work::none: break;
  }
}

bool VirtualGalleryStorage::inject_completion(hmi::camera::GalleryCompletion completion) noexcept {
  if (completion.token == 0U || completion_.token != 0U) {
    return false;
  }
  completion_ = std::move(completion);
  return true;
}

bool VirtualGalleryStorage::take_completion(GalleryCompletion& output) noexcept {
  if (completion_.token == 0U) {
    return false;
  }
  output = std::exchange(completion_, {});
  return true;
}

void VirtualGalleryStorage::cancel_and_drain() noexcept {
  frame_.reset();
  completion_ = {};
  request_token_ = 0U;
  work_ = Work::none;
  staged_ = nullptr;
  next_error_ = GalleryError::none;
}

}  // namespace hmi::platform::windows::camera
