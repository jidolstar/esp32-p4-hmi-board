#pragma once

#include <cstddef>
#include <cstdint>

#include "hmi/update/ports.hpp"

namespace hmi::update {

/**
 * 검증된 Manifest의 artifact를 staging 저장소로 재개 다운로드한다.
 *
 * caller가 제공한 scratch는 `run()` 동안만 사용하고 보관하지 않는다. 최대 4 KiB만 허용하며
 * 전체 artifact를 heap이나 task stack에 복제하지 않는다.
 */
class ArtifactDownloader {
 public:
  ArtifactDownloader(DownloadPort download, StagingPort staging,
                     CryptoPort crypto) noexcept
      : download_(download), staging_(staging), crypto_(crypto) {}

  [[nodiscard]] Error run(const Manifest& manifest, DownloadControl control,
                          std::uint8_t* scratch,
                          std::size_t scratch_size) noexcept;

 private:
  [[nodiscard]] Error digest_existing(const Manifest& manifest,
                                      std::uint64_t bytes,
                                      std::uint8_t* scratch,
                                      std::size_t scratch_size) noexcept;

  DownloadPort download_{};
  StagingPort staging_{};
  CryptoPort crypto_{};
};

static_assert(sizeof(ArtifactDownloader) <= 160U,
              "downloader must keep only non-owning ports");

}  // namespace hmi::update
