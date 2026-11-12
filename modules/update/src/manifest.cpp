#include "hmi/update/manifest.hpp"

#include <cstring>

#include "json_view.hpp"

namespace hmi::update {
namespace {

using Json = detail::json::View;

template <std::size_t Capacity>
bool copy_text(Json value, char (&output)[Capacity]) noexcept {
  std::string_view text;
  if (!value.string(text) || text.empty() || text.size() >= Capacity) {
    return false;
  }
  std::memcpy(output, text.data(), text.size());
  output[text.size()] = '\0';
  return true;
}

bool integer(Json value, std::int64_t& output) noexcept {
  std::uint64_t parsed = 0U;
  if (!value.integer(parsed) || parsed > 0x7FFFFFFFFFFFFFFFULL) {
    return false;
  }
  output = static_cast<std::int64_t>(parsed);
  return true;
}

}  // namespace

Error decode_manifest(std::string_view descriptor, std::string_view key_id,
                      std::string_view signature,
                      const ManifestBinding& binding, CryptoPort crypto,
                      Manifest& output) noexcept {
  if (descriptor.empty() || descriptor.size() > kMaximumManifestBytes ||
      key_id.empty() || key_id.size() > 64U || signature.empty() ||
      signature.size() > 128U || crypto.verify == nullptr ||
      !valid_uuid(binding.installation_id) ||
      !valid_uuid(binding.device_id) || binding.target == Target::invalid) {
    return Error::invalid_argument;
  }
  if (!crypto.verify(crypto.context, key_id,
                     reinterpret_cast<const std::uint8_t*>(descriptor.data()),
                     descriptor.size(), signature)) {
    return Error::signature;
  }

  const Json root(descriptor);
  const char* const allowed[] = {
      "schema",         "installationId", "deviceId", "target",
      "intent",         "version",        "size",     "sha256",
      "artifactPath",   "expiresAt",
  };
  std::uint64_t schema = 0U;
  std::uint64_t size = 0U;
  std::string_view target;
  std::string_view intent;
  Manifest parsed{};
  if (!root.valid() || !root.object() ||
      !root.keys(allowed, sizeof(allowed) / sizeof(allowed[0])) ||
      !root.get("schema").integer(schema) || schema != 1U ||
      !copy_text(root.get("installationId"), parsed.installation_id) ||
      !copy_text(root.get("deviceId"), parsed.device_id) ||
      !copy_text(root.get("version"), parsed.version) ||
      !copy_text(root.get("sha256"), parsed.sha256) ||
      !copy_text(root.get("artifactPath"), parsed.artifact_path) ||
      !root.get("target").string(target) ||
      !root.get("intent").string(intent) ||
      !root.get("size").integer(size) || !integer(root.get("expiresAt"), parsed.expires_at)) {
    return Error::invalid_argument;
  }
  parsed.target = target_from_name(target);
  parsed.intent = intent == "install" ? Intent::install : Intent::repair;
  parsed.size = size;
  if (!valid_uuid(parsed.installation_id) || !valid_uuid(parsed.device_id) ||
      !valid_semver(parsed.version) || !valid_sha256(parsed.sha256) ||
      !safe_relative_path(parsed.artifact_path) ||
      parsed.target == Target::invalid ||
      (intent != "install" && intent != "repair") || parsed.size == 0U ||
      parsed.size > kMaximumArtifactBytes || parsed.expires_at <= 0) {
    return Error::invalid_argument;
  }
  if (binding.installation_id != parsed.installation_id ||
      binding.device_id != parsed.device_id || binding.target != parsed.target) {
    return Error::binding;
  }
  if (!binding.allow_expired_recovery &&
      (binding.now_epoch < 1704067200LL ||
       parsed.expires_at <= binding.now_epoch ||
       parsed.expires_at - binding.now_epoch > 24LL * 60LL * 60LL + 300LL)) {
    return Error::expired;
  }
  output = parsed;
  return Error::none;
}

}  // namespace hmi::update
