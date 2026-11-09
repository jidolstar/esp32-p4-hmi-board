#include "storage.hpp"

#include <utility>

#include "internal/sd_storage.hpp"

namespace hmi::boards::jc1060p470c {
namespace {

class ScopedFileLease {
 public:
  explicit ScopedFileLease(internal::SdStorage* storage) noexcept
      : storage_(storage != nullptr && storage->acquire_file_lease() ? storage
                                                                     : nullptr) {}
  ~ScopedFileLease() {
    if (storage_ != nullptr) {
      storage_->release_file_lease();
    }
  }
  explicit operator bool() const noexcept { return storage_ != nullptr; }

 private:
  internal::SdStorage* storage_;
};

}  // namespace

Storage::ExclusiveLease::~ExclusiveLease() { release(); }

Storage::ExclusiveLease::ExclusiveLease(ExclusiveLease&& other) noexcept
    : storage_(std::exchange(other.storage_, nullptr)) {}

Storage::ExclusiveLease& Storage::ExclusiveLease::operator=(
    ExclusiveLease&& other) noexcept {
  if (this != &other) {
    release();
    storage_ = std::exchange(other.storage_, nullptr);
  }
  return *this;
}

void Storage::ExclusiveLease::release() noexcept {
  if (storage_ != nullptr) {
    storage_->release_file_lease();
    storage_ = nullptr;
  }
}

bool Storage::ready() const noexcept { return storage_->ready(); }

StorageStatus Storage::status() const noexcept { return storage_->status(); }

std::uint32_t Storage::generation() const noexcept {
  return storage_->generation();
}

bool Storage::exists(const char* path) const noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->exists(path);
}

std::uint64_t Storage::free_bytes() const noexcept {
  const ScopedFileLease lease(storage_);
  return lease ? storage_->free_bytes() : 0U;
}

bool Storage::file_size(const char* path, std::uint64_t& size) const noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->file_size(path, size);
}

bool Storage::read_file_at(const char* path, std::uint64_t offset,
                           std::uint8_t* buffer, std::size_t capacity,
                           std::size_t& bytes_read) const noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->read_file_at(path, offset, buffer, capacity,
                                         bytes_read);
}

std::FILE* Storage::open_read_stream(const char* path) const noexcept {
  return storage_->open_read_stream(path);
}

bool Storage::write_file(const char* path, const std::uint8_t* data,
                         std::size_t size) noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->write_file(path, data, size);
}

bool Storage::append_file(const char* path, const std::uint8_t* data,
                          std::size_t size) noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->append_file(path, data, size);
}

bool Storage::truncate_file(const char* path, std::uint64_t size) noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->truncate_file(path, size);
}

bool Storage::mkdirs(const char* path) noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->mkdirs(path);
}

bool Storage::remove_file(const char* path) noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->remove_file(path);
}

bool Storage::remove_tree(const char* path) noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->remove_tree(path);
}

bool Storage::rename_path(const char* from, const char* to) noexcept {
  const ScopedFileLease lease(storage_);
  return lease && storage_->rename_path(from, to);
}

Storage::ExclusiveLease Storage::acquire_exclusive_lease() noexcept {
  return storage_->acquire_file_lease() ? ExclusiveLease(storage_)
                                        : ExclusiveLease();
}

}  // namespace hmi::boards::jc1060p470c
