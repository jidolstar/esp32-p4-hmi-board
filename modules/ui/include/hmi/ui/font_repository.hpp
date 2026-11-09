#pragma once

#include <cstddef>
#include <cstdint>

#include <lvgl.h>

namespace hmi::ui {

struct FontAsset {
  const char* relative_path = nullptr;
  std::uint32_t size_bytes = 0U;
  std::uint32_t crc32 = 0U;
};

struct FontRange {
  std::uint32_t first = 0U;
  std::uint32_t last = 0U;
  std::uint8_t asset_index = 0U;
};

struct FontManifestView {
  const FontAsset* assets = nullptr;
  std::size_t asset_count = 0U;
  const FontRange* ranges = nullptr;
  std::size_t range_count = 0U;
  std::uint32_t replacement_codepoint = 0U;
};

struct FontAssetLoader {
  using Load = lv_font_t* (*)(void* context, const char* relative_path,
                              std::uint32_t size_bytes,
                              std::uint32_t crc32) noexcept;
  using Free = void (*)(void* context, lv_font_t* font) noexcept;

  void* context = nullptr;
  Load load = nullptr;
  Free free = nullptr;
};

enum class FontResolveStatus : std::uint8_t {
  ok,
  missing_glyph,
  invalid_manifest,
  load_failed,
  cache_full,
};

struct FontResolution {
  FontResolveStatus status = FontResolveStatus::invalid_manifest;
  const lv_font_t* font = nullptr;
  std::uint32_t rendered_codepoint = 0U;
  std::uint8_t asset_index = 0U;
};

/**
 * 분할 사용자 글꼴을 필요할 때만 읽고 page 수명 동안 보관하는 bounded repository다.
 *
 * manifest와 loader context는 repository보다 오래 살아야 한다. 로드된 font와 backing
 * data는 reset 또는 소멸 때까지 유지되므로 이를 참조하는 LVGL widget을 먼저 삭제해야
 * 한다. 전체 한글 글꼴을 시작 시 올리지 않고 실제 조회된 block만 page 수명 동안 보관한다.
 */
class FontRepository {
 public:
  static constexpr std::size_t kMaxCachedFonts = 176U;

  FontRepository() = default;
  FontRepository(const FontRepository&) = delete;
  FontRepository& operator=(const FontRepository&) = delete;
  ~FontRepository();

  bool initialize(FontManifestView manifest, FontAssetLoader loader) noexcept;
  void reset() noexcept;
  [[nodiscard]] FontResolution resolve(std::uint32_t codepoint) noexcept;
  [[nodiscard]] std::size_t loaded_count() const noexcept { return loaded_count_; }
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }

 private:
  struct CacheEntry {
    std::uint8_t asset_index = 0U;
    lv_font_t* font = nullptr;
  };

  [[nodiscard]] const FontRange* find_range(std::uint32_t codepoint) const noexcept;
  [[nodiscard]] lv_font_t* load_asset(std::uint8_t asset_index,
                                      FontResolveStatus& status) noexcept;

  FontManifestView manifest_{};
  FontAssetLoader loader_{};
  CacheEntry* cache_ = nullptr;
  std::size_t cache_capacity_ = 0U;
  std::size_t loaded_count_ = 0U;
  bool initialized_ = false;
};

static_assert(sizeof(FontRepository) <= 176U,
              "FontRepository must keep its bounded cache index off the page object");

/**
 * LVGL file 경로의 작은 font BIN을 한 번에 읽고 CRC를 확인한 뒤 memory FS에서 파싱한다.
 *
 * 입력 callback stack에는 파일 buffer를 두지 않으며 최대 32KiB만 checked heap에
 * 할당한다. LVGL thread에서만 동기 호출해야 한다.
 */
[[nodiscard]] lv_font_t* load_binary_font_file(const char* path,
                                                std::uint32_t size_bytes,
                                                std::uint32_t expected_crc32) noexcept;

}  // namespace hmi::ui

