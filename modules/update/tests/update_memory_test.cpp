#include <type_traits>

#include "hmi/update/artifact_download.hpp"
#include "hmi/update/manifest.hpp"
#include "hmi/update/ports.hpp"

static_assert(sizeof(hmi::update::Manifest) <= 400U);
static_assert(sizeof(hmi::update::ArtifactDownloader) <= 160U);
static_assert(std::is_trivially_copyable_v<hmi::update::Manifest>);
static_assert(std::is_trivially_copyable_v<hmi::update::FirmwareWriterPort>);
static_assert(std::is_trivially_copyable_v<hmi::update::BootControlPort>);
static_assert(sizeof(hmi::update::FirmwareWriterPort) <=
              5U * sizeof(void*));
static_assert(sizeof(hmi::update::BootControlPort) <= 5U * sizeof(void*));
static_assert(hmi::update::kMaximumIoChunkBytes == 4096U);
static_assert(hmi::update::kMaximumManifestBytes == 65536U);

int main() { return 0; }
