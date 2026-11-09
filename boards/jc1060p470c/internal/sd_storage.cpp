#include "internal/sd_storage.hpp"

#include <Arduino.h>
#include <SD_MMC.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.hpp"
#include "log.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

constexpr std::uint8_t kMaxOpenFiles = 5U;
constexpr std::uint32_t kMountRetryDelayMs = 80U;
constexpr std::uint32_t kMountFrequenciesKhz[] = {
    SDMMC_FREQ_DEFAULT,
    20U * 1000U,
    10U * 1000U,
    4U * 1000U,
};  // 카드 초기 응답이 불안정하면 clock을 낮춰 호환성을 우선한다.

}  // namespace

SdStorage::~SdStorage() { end(); }

bool SdStorage::begin() {
  if (mounted_) {
    return true;
  }

  if (!SD_MMC.setPins(config::kSdClock, config::kSdCommand, config::kSdData0,
                      config::kSdData1, config::kSdData2, config::kSdData3)) {
    HMI_LOG_ERROR("[storage] SDMMC pin setup failed\r\n");
    return false;
  }

  for (const std::uint32_t frequency_khz : kMountFrequenciesKhz) {
    HMI_LOG_INFO("[storage] SDMMC mount try freq=%ukHz\r\n",
                 static_cast<unsigned>(frequency_khz));
    if (!SD_MMC.begin(config::kSdMountPath, false, false, frequency_khz,
                      kMaxOpenFiles)) {
      SD_MMC.end();
      delay(kMountRetryDelayMs);
      continue;
    }
    if (SD_MMC.cardType() == CARD_NONE) {
      HMI_LOG_INFO("[storage] no SD card freq=%ukHz\r\n",
                   static_cast<unsigned>(frequency_khz));
      SD_MMC.end();
      delay(kMountRetryDelayMs);
      continue;
    }

    mounted_ = true;
    HMI_LOG_INFO("[storage] SD ready bytes=%llu freq=%ukHz\r\n",
                 static_cast<unsigned long long>(SD_MMC.totalBytes()),
                 static_cast<unsigned>(frequency_khz));
    return true;
  }

  HMI_LOG_ERROR("[storage] SDMMC mount failed at all frequencies\r\n");
  return false;
}

void SdStorage::end() noexcept {
  if (mounted_) {
    SD_MMC.end();
    mounted_ = false;
  }
}

bool SdStorage::ready() const noexcept { return mounted_; }

bool SdStorage::resolve_path(const char* path, char* output,
                             std::size_t capacity) const noexcept {
  if (!mounted_ || path == nullptr || output == nullptr || capacity == 0U ||
      path[0] != '/' || std::strstr(path, "..") != nullptr) {
    return false;
  }
  const int written = std::snprintf(output, capacity, "%s%s",
                                    config::kSdMountPath, path);
  return written > 0 && static_cast<std::size_t>(written) < capacity;
}

bool SdStorage::exists(const char* path) const noexcept {
  char full_path[256]{};
  struct stat info {};
  return resolve_path(path, full_path, sizeof(full_path)) &&
         ::stat(full_path, &info) == 0;
}

std::uint64_t SdStorage::free_bytes() const noexcept {
  if (!mounted_) {
    return 0U;
  }
  const std::uint64_t total = SD_MMC.totalBytes();
  const std::uint64_t used = SD_MMC.usedBytes();
  return used <= total ? total - used : 0U;
}

bool SdStorage::file_size(const char* path, std::uint64_t& size) const noexcept {
  size = 0U;
  char full_path[256]{};
  struct stat info {};
  if (!resolve_path(path, full_path, sizeof(full_path)) ||
      ::stat(full_path, &info) != 0 || (info.st_mode & S_IFDIR) != 0) {
    return false;
  }
  size = static_cast<std::uint64_t>(info.st_size);
  return true;
}

bool SdStorage::read_file_at(const char* path, std::uint64_t offset,
                             std::uint8_t* buffer, std::size_t capacity,
                             std::size_t& bytes_read) const noexcept {
  bytes_read = 0U;
  char full_path[256]{};
  if (buffer == nullptr || capacity == 0U ||
      !resolve_path(path, full_path, sizeof(full_path))) {
    return false;
  }
  std::FILE* file = std::fopen(full_path, "rb");
  if (file == nullptr) {
    return false;
  }
  const bool positioned =
      offset <= static_cast<std::uint64_t>(INT64_MAX) &&
      ::fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
  if (positioned) {
    bytes_read = std::fread(buffer, 1U, capacity, file);
  }
  const bool result = positioned && std::ferror(file) == 0;
  std::fclose(file);
  return result;
}

std::FILE* SdStorage::open_read_stream(const char* path) const noexcept {
  char full_path[256]{};
  return resolve_path(path, full_path, sizeof(full_path))
             ? std::fopen(full_path, "rb")
             : nullptr;
}

bool SdStorage::write_with_mode(const char* path, const std::uint8_t* data,
                                std::size_t size, const char* mode) noexcept {
  char full_path[256]{};
  if ((size > 0U && data == nullptr) || mode == nullptr ||
      !resolve_path(path, full_path, sizeof(full_path))) {
    return false;
  }
  std::FILE* file = std::fopen(full_path, mode);
  if (file == nullptr) {
    return false;
  }
  const bool written = size == 0U || std::fwrite(data, 1U, size, file) == size;
  const bool synced = written && std::fflush(file) == 0 &&
                      ::fsync(::fileno(file)) == 0;
  std::fclose(file);
  return synced;
}

bool SdStorage::write_file(const char* path, const std::uint8_t* data,
                           std::size_t size) noexcept {
  return write_with_mode(path, data, size, "wb");
}

bool SdStorage::append_file(const char* path, const std::uint8_t* data,
                            std::size_t size) noexcept {
  return write_with_mode(path, data, size, "ab");
}

bool SdStorage::truncate_file(const char* path, std::uint64_t size) noexcept {
  char full_path[256]{};
  if (size > static_cast<std::uint64_t>(INT64_MAX) ||
      !resolve_path(path, full_path, sizeof(full_path))) {
    return false;
  }
  std::FILE* file = std::fopen(full_path, "rb+");
  if (file == nullptr) {
    return false;
  }
  const bool result = ::ftruncate(::fileno(file), static_cast<off_t>(size)) == 0 &&
                      std::fflush(file) == 0 && ::fsync(::fileno(file)) == 0;
  std::fclose(file);
  return result;
}

bool SdStorage::mkdirs(const char* path) noexcept {
  char full_path[256]{};
  if (!resolve_path(path, full_path, sizeof(full_path))) {
    return false;
  }
  const std::size_t mount_length = std::strlen(config::kSdMountPath);
  for (char* cursor = full_path + mount_length + 1; *cursor != '\0'; ++cursor) {
    if (*cursor != '/') {
      continue;
    }
    *cursor = '\0';
    struct stat info {};
    const bool ready = ::stat(full_path, &info) == 0
                           ? (info.st_mode & S_IFDIR) != 0
                           : ::mkdir(full_path, 0755) == 0;
    *cursor = '/';
    if (!ready) {
      return false;
    }
  }
  struct stat info {};
  return ::stat(full_path, &info) == 0
             ? (info.st_mode & S_IFDIR) != 0
             : ::mkdir(full_path, 0755) == 0;
}

bool SdStorage::remove_file(const char* path) noexcept {
  char full_path[256]{};
  if (!resolve_path(path, full_path, sizeof(full_path))) {
    return false;
  }
  struct stat info {};
  return ::stat(full_path, &info) != 0 || std::remove(full_path) == 0;
}

bool SdStorage::remove_tree_impl(const char* full_path, unsigned depth) noexcept {
  constexpr unsigned kMaxDepth = 12U;
  if (full_path == nullptr || depth > kMaxDepth) {
    return false;
  }
  struct stat info {};
  if (::stat(full_path, &info) != 0) {
    return true;
  }
  if ((info.st_mode & S_IFDIR) == 0) {
    return std::remove(full_path) == 0;
  }
  DIR* directory = ::opendir(full_path);
  if (directory == nullptr) {
    return false;
  }
  bool result = true;
  while (result) {
    dirent* entry = ::readdir(directory);
    if (entry == nullptr) {
      break;
    }
    if (std::strcmp(entry->d_name, ".") == 0 ||
        std::strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    char child[320]{};
    const int written = std::snprintf(child, sizeof(child), "%s/%s", full_path,
                                      entry->d_name);
    result = written > 0 && static_cast<std::size_t>(written) < sizeof(child) &&
             remove_tree_impl(child, depth + 1U);
  }
  ::closedir(directory);
  return result && ::rmdir(full_path) == 0;
}

bool SdStorage::remove_tree(const char* path) noexcept {
  char full_path[256]{};
  if (!resolve_path(path, full_path, sizeof(full_path)) ||
      std::strcmp(full_path, config::kSdMountPath) == 0) {
    return false;
  }
  return remove_tree_impl(full_path, 0U);
}

bool SdStorage::rename_path(const char* from, const char* to) noexcept {
  char full_from[256]{};
  char full_to[256]{};
  return resolve_path(from, full_from, sizeof(full_from)) &&
         resolve_path(to, full_to, sizeof(full_to)) &&
         std::rename(full_from, full_to) == 0;
}

}  // namespace hmi::boards::jc1060p470c::internal
