#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hmi::update {

inline constexpr std::size_t kMaximumManifestBytes = 64U * 1024U;
inline constexpr std::size_t kMaximumIoChunkBytes = 4U * 1024U;
inline constexpr std::uint64_t kMaximumArtifactBytes = 512ULL * 1024ULL * 1024ULL;

enum class Target : std::uint8_t { firmware, assets, configuration, invalid };
enum class Intent : std::uint8_t { install, repair };
enum class Error : std::uint8_t {
  none,
  invalid_argument,
  signature,
  binding,
  expired,
  unavailable,
  busy,
  storage,
  network,
  digest,
  cancelled,
  paused,
};

/** 검증을 마친 update descriptor의 bounded metadata다. 원문 JSON은 보관하지 않는다. */
struct Manifest {
  char installation_id[37]{};
  char device_id[37]{};
  char version[41]{};
  char sha256[65]{};
  char artifact_path[192]{};
  std::uint64_t size = 0U;
  std::int64_t expires_at = 0;
  Target target = Target::invalid;
  Intent intent = Intent::install;
};

[[nodiscard]] Target target_from_name(std::string_view value) noexcept;
[[nodiscard]] std::string_view target_name(Target value) noexcept;
[[nodiscard]] bool valid_uuid(std::string_view value) noexcept;
[[nodiscard]] bool valid_sha256(std::string_view value) noexcept;
[[nodiscard]] bool valid_semver(std::string_view value) noexcept;
[[nodiscard]] bool safe_relative_path(std::string_view value) noexcept;

static_assert(sizeof(Manifest) <= 400U,
              "update manifest metadata must remain bounded");

}  // namespace hmi::update
