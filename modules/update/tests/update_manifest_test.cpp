#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "hmi/update/manifest.hpp"

namespace {

constexpr std::string_view kInstallation =
    "11111111-1111-1111-1111-111111111111";
constexpr std::string_view kDevice = "22222222-2222-2222-2222-222222222222";
constexpr std::string_view kSha =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

bool verify(void*, std::string_view key, const std::uint8_t*, std::size_t,
            std::string_view signature) {
  return key == "release-key" && signature == "trusted";
}

hmi::update::CryptoPort crypto() {
  hmi::update::CryptoPort value{};
  value.verify = &verify;
  return value;
}

std::string descriptor(std::string_view extra = {}) {
  return std::string("{\"schema\":1,\"installationId\":\"") +
         std::string(kInstallation) + "\",\"deviceId\":\"" +
         std::string(kDevice) +
         "\",\"target\":\"firmware\",\"intent\":\"install\"," +
         "\"version\":\"1.2.3\",\"size\":8,\"sha256\":\"" +
         std::string(kSha) +
         "\",\"artifactPath\":\"firmware/release.bin\"," +
         "\"expiresAt\":1704070800" + std::string(extra) + "}";
}

hmi::update::ManifestBinding binding() {
  return {kInstallation, kDevice, hmi::update::Target::firmware,
          1704067200, false};
}

}  // namespace

int main() {
  using namespace hmi::update;

  Manifest parsed{};
  assert(decode_manifest(descriptor(), "release-key", "trusted", binding(),
                         crypto(), parsed) == Error::none);
  assert(std::string_view(parsed.installation_id) == kInstallation);
  assert(std::string_view(parsed.device_id) == kDevice);
  assert(std::string_view(parsed.version) == "1.2.3");
  assert(std::string_view(parsed.artifact_path) == "firmware/release.bin");
  assert(parsed.size == 8U);
  assert(parsed.target == Target::firmware);

  Manifest unchanged{};
  std::strcpy(unchanged.version, "9.9.9");
  assert(decode_manifest(descriptor(), "release-key", "bad", binding(),
                         crypto(), unchanged) == Error::signature);
  assert(std::string_view(unchanged.version) == "9.9.9");

  auto wrong_device = binding();
  wrong_device.device_id = "33333333-3333-3333-3333-333333333333";
  assert(decode_manifest(descriptor(), "release-key", "trusted", wrong_device,
                         crypto(), unchanged) == Error::binding);

  auto expired = binding();
  expired.now_epoch = 1704070800;
  assert(decode_manifest(descriptor(), "release-key", "trusted", expired,
                         crypto(), unchanged) == Error::expired);
  expired.allow_expired_recovery = true;
  assert(decode_manifest(descriptor(), "release-key", "trusted", expired,
                         crypto(), unchanged) == Error::none);

  assert(decode_manifest(descriptor(",\"unknown\":1"), "release-key",
                         "trusted", binding(), crypto(), unchanged) ==
         Error::invalid_argument);

  auto duplicate = descriptor();
  const auto close = duplicate.rfind('}');
  duplicate.insert(close, ",\"size\":9");
  assert(decode_manifest(duplicate, "release-key", "trusted", binding(),
                         crypto(), unchanged) == Error::invalid_argument);

  const std::string deep =
      "{\"schema\":1,\"installationId\":\"11111111-1111-1111-1111-111111111111\","
      "\"deviceId\":\"22222222-2222-2222-2222-222222222222\",\"target\":{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":{\"f\":{\"g\":{\"h\":{}}}}}}}}},"
      "\"intent\":\"install\",\"version\":\"1.2.3\",\"size\":8,"
      "\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
      "\"artifactPath\":\"firmware/release.bin\",\"expiresAt\":1704070800}";
  assert(decode_manifest(deep, "release-key", "trusted", binding(), crypto(),
                         unchanged) == Error::invalid_argument);

  assert(valid_uuid(kInstallation));
  assert(!valid_uuid("not-a-uuid"));
  assert(valid_sha256(kSha));
  assert(!valid_sha256("ABC"));
  assert(valid_semver("10.0.3"));
  assert(!valid_semver("01.0.0"));
  assert(safe_relative_path("assets/images/ui.bin"));
  assert(!safe_relative_path("assets/../secret"));

  return 0;
}
