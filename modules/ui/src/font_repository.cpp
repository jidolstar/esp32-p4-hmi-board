#include "hmi/ui/font_repository.hpp"

#include <cstring>
#include <memory>
#include <new>

namespace hmi::ui {
namespace {

constexpr std::uint32_t kMaxBinaryFontBytes = 32U * 1024U;

struct MemoryFontFile {
  const std::uint8_t* bytes = nullptr;
  std::size_t size = 0U;
  std::size_t offset = 0U;
};

MemoryFontFile active_file;
lv_fs_drv_t memory_driver;
bool memory_driver_registered = false;

void* open_memory_font(lv_fs_drv_t*, const char* path, lv_fs_mode_t mode) {
  if (mode != LV_FS_MODE_RD || std::strcmp(path, "/font.bin") != 0 ||
      active_file.bytes == nullptr) {
    return nullptr;
  }
  active_file.offset = 0U;
  return &active_file;
}

lv_fs_res_t close_memory_font(lv_fs_drv_t*, void*) { return LV_FS_RES_OK; }

lv_fs_res_t read_memory_font(lv_fs_drv_t*, void* file, void* buffer,
                             std::uint32_t count, std::uint32_t* read) {
  auto* current = static_cast<MemoryFontFile*>(file);
  if (current == nullptr || current->offset > current->size ||
      count > current->size - current->offset) {
    if (read != nullptr) {
      *read = 0U;
    }
    return LV_FS_RES_FS_ERR;
  }
  std::memcpy(buffer, current->bytes + current->offset, count);
  current->offset += count;
  if (read != nullptr) {
    *read = count;
  }
  return LV_FS_RES_OK;
}

lv_fs_res_t seek_memory_font(lv_fs_drv_t*, void* file, std::uint32_t offset,
                             lv_fs_whence_t origin) {
  auto* current = static_cast<MemoryFontFile*>(file);
  if (current == nullptr) {
    return LV_FS_RES_FS_ERR;
  }
  const std::size_t base = origin == LV_FS_SEEK_SET
                               ? 0U
                               : origin == LV_FS_SEEK_CUR ? current->offset
                                                         : current->size;
  if (base > current->size || offset > current->size - base) {
    return LV_FS_RES_FS_ERR;
  }
  current->offset = base + offset;
  return LV_FS_RES_OK;
}

lv_fs_res_t tell_memory_font(lv_fs_drv_t*, void* file,
                             std::uint32_t* position) {
  auto* current = static_cast<MemoryFontFile*>(file);
  if (current == nullptr || position == nullptr ||
      current->offset > UINT32_MAX) {
    return LV_FS_RES_FS_ERR;
  }
  *position = static_cast<std::uint32_t>(current->offset);
  return LV_FS_RES_OK;
}

bool ensure_memory_driver() noexcept {
  if (memory_driver_registered) {
    return true;
  }
  if (lv_fs_get_drv('U') != nullptr) {
    return false;
  }
  lv_fs_drv_init(&memory_driver);
  memory_driver.letter = 'U';
  memory_driver.open_cb = &open_memory_font;
  memory_driver.close_cb = &close_memory_font;
  memory_driver.read_cb = &read_memory_font;
  memory_driver.seek_cb = &seek_memory_font;
  memory_driver.tell_cb = &tell_memory_font;
  lv_fs_drv_register(&memory_driver);
  memory_driver_registered = true;
  return true;
}

std::uint32_t crc32(const std::uint8_t* bytes, std::size_t size) noexcept {
  std::uint32_t value = 0xFFFFFFFFU;
  for (std::size_t index = 0U; index < size; ++index) {
    value ^= bytes[index];
    for (unsigned bit = 0U; bit < 8U; ++bit) {
      value = (value >> 1U) ^ (0xEDB88320U & (0U - (value & 1U)));
    }
  }
  return value ^ 0xFFFFFFFFU;
}

}  // namespace

lv_font_t* load_binary_font_file(const char* path, std::uint32_t size_bytes,
                                 std::uint32_t expected_crc32) noexcept {
  if (path == nullptr || size_bytes == 0U ||
      size_bytes > kMaxBinaryFontBytes || !ensure_memory_driver()) {
    return nullptr;
  }
  std::unique_ptr<std::uint8_t[]> bytes(
      new (std::nothrow) std::uint8_t[size_bytes]);
  if (!bytes) {
    return nullptr;
  }
  lv_fs_file_t file{};
  if (lv_fs_open(&file, path, LV_FS_MODE_RD) != LV_FS_RES_OK) {
    return nullptr;
  }
  std::uint32_t actual = 0U;
  const lv_fs_res_t read_result =
      lv_fs_read(&file, bytes.get(), size_bytes, &actual);
  lv_fs_close(&file);
  if (read_result != LV_FS_RES_OK || actual != size_bytes ||
      crc32(bytes.get(), size_bytes) != expected_crc32) {
    return nullptr;
  }

  // lv_font_load가 반환하기 전에 bitmap과 descriptor를 LVGL heap으로 복사한다.
  active_file = {bytes.get(), size_bytes, 0U};
  lv_font_t* font = lv_font_load("U:/font.bin");
  active_file = {};
  return font;
}

FontRepository::~FontRepository() { reset(); }

bool FontRepository::initialize(FontManifestView manifest,
                                FontAssetLoader loader) noexcept {
  reset();
  if (manifest.assets == nullptr || manifest.asset_count == 0U ||
      manifest.asset_count > kMaxCachedFonts || manifest.ranges == nullptr ||
      manifest.range_count == 0U || loader.load == nullptr || loader.free == nullptr) {
    return false;
  }
  for (std::size_t index = 0U; index < manifest.range_count; ++index) {
    const FontRange& range = manifest.ranges[index];
    if (range.first > range.last || range.asset_index >= manifest.asset_count) {
      return false;
    }
  }
  cache_ = new (std::nothrow) CacheEntry[manifest.asset_count]{};
  if (cache_ == nullptr) {
    return false;
  }
  cache_capacity_ = manifest.asset_count;
  manifest_ = manifest;
  loader_ = loader;
  initialized_ = find_range(manifest.replacement_codepoint) != nullptr;
  if (!initialized_) {
    delete[] cache_;
    cache_ = nullptr;
    cache_capacity_ = 0U;
    manifest_ = {};
    loader_ = {};
  }
  return initialized_;
}

void FontRepository::reset() noexcept {
  // LVGL binary font loader가 할당한 bitmap/dsc도 같은 free callback으로 정리한다.
  for (std::size_t index = 0U; index < loaded_count_; ++index) {
    if (cache_[index].font != nullptr && loader_.free != nullptr) {
      loader_.free(loader_.context, cache_[index].font);
    }
    cache_[index] = {};
  }
  delete[] cache_;
  cache_ = nullptr;
  cache_capacity_ = 0U;
  loaded_count_ = 0U;
  initialized_ = false;
  manifest_ = {};
  loader_ = {};
}

const FontRange* FontRepository::find_range(std::uint32_t codepoint) const noexcept {
  for (std::size_t index = 0U; index < manifest_.range_count; ++index) {
    const FontRange& range = manifest_.ranges[index];
    if (codepoint >= range.first && codepoint <= range.last) {
      return &range;
    }
  }
  return nullptr;
}

lv_font_t* FontRepository::load_asset(std::uint8_t asset_index,
                                      FontResolveStatus& status) noexcept {
  for (std::size_t index = 0U; index < loaded_count_; ++index) {
    if (cache_[index].asset_index == asset_index) {
      status = FontResolveStatus::ok;
      return cache_[index].font;
    }
  }
  if (loaded_count_ >= cache_capacity_) {
    status = FontResolveStatus::cache_full;
    return nullptr;
  }
  const FontAsset& asset = manifest_.assets[asset_index];
  if (asset.relative_path == nullptr || asset.relative_path[0] == '\0') {
    status = FontResolveStatus::invalid_manifest;
    return nullptr;
  }
  lv_font_t* font = loader_.load(loader_.context, asset.relative_path,
                                 asset.size_bytes, asset.crc32);
  if (font == nullptr) {
    status = FontResolveStatus::load_failed;
    return nullptr;
  }
  cache_[loaded_count_++] = {asset_index, font};
  status = FontResolveStatus::ok;
  return font;
}

FontResolution FontRepository::resolve(std::uint32_t codepoint) noexcept {
  if (!initialized_) {
    return {FontResolveStatus::invalid_manifest, nullptr, 0U, 0U};
  }
  const FontRange* range = find_range(codepoint);
  FontResolveStatus result_status = FontResolveStatus::ok;
  std::uint32_t rendered = codepoint;
  if (range == nullptr) {
    range = find_range(manifest_.replacement_codepoint);
    result_status = FontResolveStatus::missing_glyph;
    rendered = manifest_.replacement_codepoint;
  }
  if (range == nullptr) {
    return {FontResolveStatus::invalid_manifest, nullptr, 0U, 0U};
  }

  FontResolveStatus load_status = FontResolveStatus::ok;
  lv_font_t* font = load_asset(range->asset_index, load_status);
  if (font == nullptr) {
    return {load_status, nullptr, rendered, range->asset_index};
  }
  return {result_status, font, rendered, range->asset_index};
}

}  // namespace hmi::ui

