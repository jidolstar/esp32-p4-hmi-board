#include "internal/asset_usb_updater.hpp"

#include <Arduino.h>
#include <mbedtls/base64.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "internal/sd_storage.hpp"
#include "log.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

constexpr std::uint32_t kMaxFileBytes = 16U * 1024U * 1024U;
constexpr std::uint32_t kMaxBundleBytes = 64U * 1024U * 1024U;
// 세분화된 사용자 폰트 블록과 마지막 manifest까지 bounded counter로 수용한다.
constexpr unsigned kMaxFiles = 513U;
constexpr std::size_t kSerialRxBytes = 4096U;

bool is_hex_lower(char value) noexcept {
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

std::uint8_t hex_value(char value) noexcept {
  return static_cast<std::uint8_t>(value <= '9' ? value - '0'
                                                       : value - 'a' + 10);
}

bool is_path_character(char value) noexcept {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9') || value == '_' || value == '-' ||
         value == '.';
}

bool has_allowed_suffix(const char* path) noexcept {
  constexpr std::array<const char*, 5U> kExtensions = {
      ".bin", ".json", ".png", ".svg", ".wav"};
  const std::size_t length = std::strlen(path);
  for (const char* extension : kExtensions) {
    const std::size_t tail = std::strlen(extension);
    if (length > tail && std::strcmp(path + length - tail, extension) == 0) {
      return true;
    }
  }
  return false;
}

bool valid_profile_id(const char* value) noexcept {
  if (value == nullptr || value[0] == '\0' || std::strlen(value) >= 32U) {
    return false;
  }
  for (const char* cursor = value; *cursor != '\0'; ++cursor) {
    if (!((*cursor >= 'a' && *cursor <= 'z') ||
          (*cursor >= '0' && *cursor <= '9') || *cursor == '-')) {
      return false;
    }
  }
  return true;
}

}  // namespace

AssetUsbUpdater::AssetUsbUpdater(SdStorage& storage) noexcept : storage_(storage) {
  mbedtls_sha256_init(&file_sha_);
}

AssetUsbUpdater::~AssetUsbUpdater() { mbedtls_sha256_free(&file_sha_); }

AssetUpdateStatus AssetUsbUpdater::status() const noexcept {
  const std::uint32_t current_bytes =
      file_open_ ? file_offset_ + static_cast<std::uint32_t>(window_used_) : 0U;
  const std::uint32_t received =
      std::min(expected_total_bytes_, accepted_bytes_ + current_bytes);
  const std::uint16_t current_file = static_cast<std::uint16_t>(
      accepted_file_count_ + (file_open_ ? 1U : 0U));
  return {phase_, preparation_, current_file, expected_file_count_, received,
          expected_total_bytes_};
}

bool AssetUsbUpdater::make_transaction_paths(const char* active_root) noexcept {
  if (active_root == nullptr || std::strncmp(active_root, "/hmi/", 5U) != 0 ||
      std::strlen(active_root) >= sizeof(active_root_) ||
      std::strstr(active_root, "..") != nullptr) {
    return false;
  }
  std::strcpy(active_root_, active_root);
  const auto make_path = [active_root](char* output, std::size_t capacity,
                                       const char* suffix) noexcept {
    const int written = std::snprintf(output, capacity, "%s%s", active_root,
                                      suffix);
    return written > 0 && static_cast<std::size_t>(written) < capacity;
  };
  return make_path(incoming_root_, sizeof(incoming_root_), ".__incoming") &&
         make_path(backup_root_, sizeof(backup_root_), ".__backup") &&
         make_path(journal_path_, sizeof(journal_path_), ".__journal");
}

bool AssetUsbUpdater::begin(const char* profile_id,
                            const char* active_root) noexcept {
  if (!valid_profile_id(profile_id) || active_root == nullptr) {
    return false;
  }
  if (enabled_) {
    return std::strcmp(profile_id_, profile_id) == 0 &&
           std::strcmp(active_root_, active_root) == 0;
  }
  if (!make_transaction_paths(active_root)) {
    return false;
  }
  std::strcpy(profile_id_, profile_id);
  if (Serial.setRxBufferSize(kSerialRxBytes) < kSerialRxBytes) {
    profile_id_[0] = '\0';
    active_root_[0] = '\0';
    return false;
  }
  // SD 오류가 있어도 CDC receiver는 살아 있어야 PC가 무응답 대신 원인을 받는다.
  storage_available_ = storage_.ready() && recover_transaction();
  enabled_ = true;
  HMI_LOG_INFO("[asset-usb] ready profile=%s root=%s storage=%s\r\n",
               profile_id_, active_root_,
               storage_available_ ? "ready" : "unavailable");
  return true;
}

bool AssetUsbUpdater::decode_sha(const char* text,
                                 std::uint8_t* output) noexcept {
  if (text == nullptr || output == nullptr || std::strlen(text) != 64U) {
    return false;
  }
  for (std::size_t index = 0U; index < 32U; ++index) {
    const char high = text[index * 2U];
    const char low = text[index * 2U + 1U];
    if (!is_hex_lower(high) || !is_hex_lower(low)) {
      return false;
    }
    output[index] = static_cast<std::uint8_t>((hex_value(high) << 4U) |
                                               hex_value(low));
  }
  return true;
}

bool AssetUsbUpdater::valid_relative_path(const char* path) const noexcept {
  if (path == nullptr || path[0] == '\0' || path[0] == '/' ||
      std::strlen(path) >= sizeof(relative_)) {
    return false;
  }
  if (std::strcmp(path, "asset_manifest.json") == 0) {
    return true;
  }
  if (!has_allowed_suffix(path)) {
    return false;
  }
  unsigned depth = 1U;
  const char* segment = path;
  for (const char* cursor = path;; ++cursor) {
    if (*cursor != '/' && *cursor != '\0') {
      if (!is_path_character(*cursor)) {
        return false;
      }
      continue;
    }
    const std::size_t length = static_cast<std::size_t>(cursor - segment);
    if (length == 0U || (length == 1U && segment[0] == '.') ||
        (length == 2U && segment[0] == '.' && segment[1] == '.')) {
      return false;
    }
    if (*cursor == '\0') {
      break;
    }
    if (++depth > 10U) {
      return false;
    }
    segment = cursor + 1;
  }
  return true;
}

bool AssetUsbUpdater::recover_transaction() noexcept {
  const bool journal = storage_.exists(journal_path_);
  const bool backup = storage_.exists(backup_root_);
  const bool active = storage_.exists(active_root_);
  if (journal) {
    if (backup) {
      if (active && !storage_.remove_tree(active_root_)) {
        return false;
      }
      if (!storage_.rename_path(backup_root_, active_root_)) {
        return false;
      }
    }
    if (storage_.exists(incoming_root_) &&
        !storage_.remove_tree(incoming_root_)) {
      return false;
    }
    return storage_.remove_file(journal_path_);
  }
  if (backup && active && !storage_.remove_tree(backup_root_)) {
    return false;
  }
  if (backup && !active && !storage_.rename_path(backup_root_, active_root_)) {
    return false;
  }
  return !storage_.exists(incoming_root_) ||
         storage_.remove_tree(incoming_root_);
}

bool AssetUsbUpdater::begin_transaction() noexcept {
  return recover_transaction() && storage_.mkdirs(incoming_root_);
}

bool AssetUsbUpdater::commit_transaction() noexcept {
  constexpr std::uint8_t kMarker[] = {'A', 'S', 'S', 'E', 'T', '2'};
  if (!storage_.write_file(journal_path_, kMarker, sizeof(kMarker))) {
    return false;
  }
  const bool had_active = storage_.exists(active_root_);
  if (had_active && !storage_.rename_path(active_root_, backup_root_)) {
    recover_transaction();
    return false;
  }
  if (!storage_.rename_path(incoming_root_, active_root_)) {
    recover_transaction();
    return false;
  }
  if (!storage_.remove_file(journal_path_)) {
    recover_transaction();
    return false;
  }
  if (had_active && storage_.exists(backup_root_)) {
    storage_.remove_tree(backup_root_);
  }
  return true;
}

void AssetUsbUpdater::tick() noexcept {
  if (!enabled_) {
    return;
  }
  if (phase_ == AssetUpdatePhase::preparing &&
      static_cast<std::uint32_t>(millis() - last_input_ms_) >
          kPreparationTimeoutMs) {
    cancel_preparation();
  } else if (active_ &&
      static_cast<std::uint32_t>(millis() - last_input_ms_) > kTimeoutMs) {
    abort("timeout");
  }
  for (unsigned processed = 0U;
       processed < 512U && Serial.available() > 0; ++processed) {
    const int input = Serial.read();
    last_input_ms_ = millis();
    if (input == '\n') {
      if (overflow_) {
        file_open_ ? request_retry() : abort("line_too_long");
      } else {
        line_[line_length_] = '\0';
        handle_line();
      }
      line_length_ = 0U;
      overflow_ = false;
    } else if (input != '\r') {
      if (line_length_ + 1U < sizeof(line_)) {
        line_[line_length_++] = static_cast<char>(input);
      } else {
        overflow_ = true;
      }
    }
  }
}

void AssetUsbUpdater::handle_line() noexcept {
  if (std::strncmp(line_, "ASSET_HELLO 2 ", 14U) == 0) {
    const char* requested_profile = line_ + 14U;
    if (std::strcmp(requested_profile, profile_id_) != 0) {
      Serial.printf("ASSET_ERROR profile_mismatch %s\n", profile_id_);
    } else if (!storage_available_) {
      Serial.print("ASSET_ERROR storage_unavailable\n");
    } else {
      Serial.printf("ASSET_V2 %s\n", profile_id_);
    }
    return;
  }
  if (std::strcmp(line_, "ASSET_STATUS") == 0) {
    send_status();
    return;
  }
  if (std::strncmp(line_, "ASSET_PREPARE ", 14U) == 0) {
    begin_preparation();
    return;
  }
  if (std::strcmp(line_, "ASSET_CANCEL") == 0) {
    cancel_preparation();
    return;
  }
  if (std::strncmp(line_, "ASSET_BEGIN ", 12U) == 0) {
    if (!storage_available_) {
      Serial.print("ASSET_ERROR storage_unavailable\n");
    } else if (active_) {
      Serial.print("ASSET_BUSY\n");
    } else {
      begin_bundle();
    }
    return;
  }
  if (!active_) {
    return;
  }
  if (std::strcmp(line_, "ASSET_ABORT") == 0) {
    abort("host_abort");
    return;
  }
  if (recovering_) {
    if (std::strncmp(line_, "ASSET_REWIND ", 13U) == 0) {
      rewind_file();
    }
    return;
  }
  if (std::strncmp(line_, "ASSET_FILE ", 11U) == 0) {
    begin_file();
  } else if (std::strncmp(line_, "ASSET_DATA ", 11U) == 0) {
    append_data();
  } else if (std::strncmp(line_, "ASSET_REWIND ", 13U) == 0) {
    rewind_file();
  } else if (std::strcmp(line_, "ASSET_END_FILE") == 0) {
    finish_file();
  } else if (std::strcmp(line_, "ASSET_COMMIT") == 0) {
    commit_bundle();
  } else if (file_open_) {
    request_retry();
  } else {
    abort("unknown_command");
  }
}

void AssetUsbUpdater::begin_preparation() noexcept {
  char step[16]{};
  char extra = '\0';
  if (active_) {
    Serial.print("ASSET_BUSY\n");
    return;
  }
  if (std::sscanf(line_, "ASSET_PREPARE %15s %c", step, &extra) != 1) {
    Serial.print("ASSET_ERROR invalid_prepare\n");
    return;
  }
  if (std::strcmp(step, "fonts") == 0) {
    preparation_ = AssetUpdatePreparationStep::fonts;
  } else if (std::strcmp(step, "assets") == 0) {
    preparation_ = AssetUpdatePreparationStep::assets;
  } else {
    Serial.print("ASSET_ERROR invalid_prepare_stage\n");
    return;
  }
  phase_ = AssetUpdatePhase::preparing;
  last_input_ms_ = millis();
  Serial.printf("ASSET_PREPARING %s\n", step);
}

void AssetUsbUpdater::cancel_preparation() noexcept {
  if (active_) {
    abort("host_cancel");
    return;
  }
  preparation_ = AssetUpdatePreparationStep::none;
  phase_ = AssetUpdatePhase::failed;
  Serial.print("ASSET_CANCELLED\n");
  HMI_LOG_ERROR("[asset-usb] preparation cancelled\r\n");
}

void AssetUsbUpdater::begin_bundle() noexcept {
  char bundle_id[65]{};
  char extra = '\0';
  unsigned count = 0U;
  unsigned long total = 0U;
  std::uint8_t bundle_digest[32]{};
  if (std::sscanf(line_, "ASSET_BEGIN %64s %u %lu %c", bundle_id, &count,
                  &total, &extra) != 3 ||
      !decode_sha(bundle_id, bundle_digest) || count == 0U ||
      count > kMaxFiles || total == 0U || total > kMaxBundleBytes ||
      storage_.free_bytes() < static_cast<std::uint64_t>(total) + 4096U ||
      !begin_transaction()) {
    Serial.print("ASSET_ERROR invalid_begin_or_storage\n");
    return;
  }
  expected_file_count_ = static_cast<std::uint16_t>(count);
  expected_total_bytes_ = static_cast<std::uint32_t>(total);
  accepted_file_count_ = 0U;
  accepted_bytes_ = 0U;
  previous_relative_[0] = '\0';
  manifest_received_ = false;
  recovering_ = false;
  file_open_ = false;
  active_ = true;
  phase_ = AssetUpdatePhase::receiving;
  preparation_ = AssetUpdatePreparationStep::none;
  last_input_ms_ = millis();
  Serial.print("ASSET_READY\n");
}

void AssetUsbUpdater::begin_file() noexcept {
  unsigned long size = 0U;
  char extra = '\0';
  if (file_open_ || accepted_file_count_ >= expected_file_count_ ||
      std::sscanf(line_, "ASSET_FILE %119s %lu %64s %c", relative_, &size,
                  expected_sha_text_, &extra) != 3 ||
      size == 0U || size > kMaxFileBytes ||
      !valid_relative_path(relative_) ||
      !decode_sha(expected_sha_text_, expected_sha_)) {
    abort("invalid_file");
    return;
  }
  const bool manifest = std::strcmp(relative_, "asset_manifest.json") == 0;
  if ((manifest && accepted_file_count_ + 1U != expected_file_count_) ||
      (!manifest && accepted_file_count_ + 1U == expected_file_count_) ||
      (!manifest && previous_relative_[0] != '\0' &&
       std::strcmp(previous_relative_, relative_) >= 0)) {
    abort("manifest_order");
    return;
  }
  const int written = std::snprintf(target_path_, sizeof(target_path_), "%s/%s",
                                    incoming_root_, relative_);
  if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(target_path_)) {
    abort("path_too_long");
    return;
  }
  char parent[sizeof(target_path_)]{};
  std::strcpy(parent, target_path_);
  char* slash = std::strrchr(parent, '/');
  if (slash == nullptr) {
    abort("invalid_parent");
    return;
  }
  *slash = '\0';
  if (!storage_.mkdirs(parent) ||
      !storage_.write_file(target_path_, nullptr, 0U)) {
    abort("file_create");
    return;
  }
  file_size_ = static_cast<std::uint32_t>(size);
  file_offset_ = 0U;
  window_used_ = 0U;
  window_chunks_ = 0U;
  recovering_ = false;
  mbedtls_sha256_starts(&file_sha_, 0);
  file_open_ = true;
  Serial.print("ASSET_FILE_READY\n");
}

void AssetUsbUpdater::append_data() noexcept {
  if (!file_open_) {
    abort("no_file");
    return;
  }
  char* end = nullptr;
  const unsigned long offset = std::strtoul(line_ + 11U, &end, 10);
  if (end == line_ + 11U || *end != ' ' ||
      offset != file_offset_ + window_used_ ||
      window_chunks_ >= kWindowChunks) {
    request_retry();
    return;
  }
  const char* encoded = end + 1;
  std::array<std::uint8_t, kChunkBytes + 4U> chunk{};
  std::size_t decoded = 0U;
  if (mbedtls_base64_decode(
          chunk.data(), chunk.size(), &decoded,
          reinterpret_cast<const unsigned char*>(encoded),
          std::strlen(encoded)) != 0 ||
      decoded == 0U || decoded > kChunkBytes ||
      decoded > sizeof(window_) - window_used_ ||
      file_offset_ + window_used_ + decoded > file_size_) {
    request_retry();
    return;
  }
  std::memcpy(window_ + window_used_, chunk.data(), decoded);
  window_used_ += decoded;
  ++window_chunks_;
  if (window_chunks_ == kWindowChunks ||
      file_offset_ + window_used_ == file_size_) {
    if (!flush_window()) {
      abort("sd_write");
      return;
    }
    Serial.printf("ASSET_ACK %lu\n",
                  static_cast<unsigned long>(file_offset_));
  }
}

bool AssetUsbUpdater::flush_window() noexcept {
  if (window_used_ == 0U ||
      !storage_.append_file(target_path_, window_, window_used_)) {
    return false;
  }
  mbedtls_sha256_update(&file_sha_, window_, window_used_);
  file_offset_ += static_cast<std::uint32_t>(window_used_);
  window_used_ = 0U;
  window_chunks_ = 0U;
  return true;
}

void AssetUsbUpdater::request_retry() noexcept {
  window_used_ = 0U;
  window_chunks_ = 0U;
  recovering_ = true;
  Serial.printf("ASSET_RETRY %lu\n",
                static_cast<unsigned long>(file_offset_));
}

bool AssetUsbUpdater::recompute_file_sha(std::uint32_t bytes) noexcept {
  mbedtls_sha256_starts(&file_sha_, 0);
  std::array<std::uint8_t, 512U> buffer{};
  for (std::uint32_t offset = 0U; offset < bytes;) {
    const std::size_t count =
        std::min<std::size_t>(buffer.size(), bytes - offset);
    std::size_t actual = 0U;
    if (!storage_.read_file_at(target_path_, offset, buffer.data(), count,
                               actual) ||
        actual != count) {
      return false;
    }
    mbedtls_sha256_update(&file_sha_, buffer.data(), actual);
    offset += static_cast<std::uint32_t>(actual);
  }
  return true;
}

void AssetUsbUpdater::rewind_file() noexcept {
  if (!file_open_) {
    abort("no_file");
    return;
  }
  char* end = nullptr;
  const unsigned long requested = std::strtoul(line_ + 13U, &end, 10);
  if (end == line_ + 13U || *end != '\0' || requested > file_offset_ ||
      !storage_.truncate_file(target_path_, requested) ||
      !recompute_file_sha(static_cast<std::uint32_t>(requested))) {
    abort("rewind_failed");
    return;
  }
  file_offset_ = static_cast<std::uint32_t>(requested);
  window_used_ = 0U;
  window_chunks_ = 0U;
  recovering_ = false;
  Serial.printf("ASSET_REWOUND %lu\n", requested);
}

bool AssetUsbUpdater::verify_stored_file() noexcept {
  std::uint64_t stored_size = 0U;
  if (!storage_.file_size(target_path_, stored_size) ||
      stored_size != file_size_) {
    return false;
  }
  mbedtls_sha256_context check{};
  mbedtls_sha256_init(&check);
  mbedtls_sha256_starts(&check, 0);
  std::array<std::uint8_t, 512U> buffer{};
  bool result = true;
  for (std::uint32_t offset = 0U; result && offset < file_size_;) {
    const std::size_t count =
        std::min<std::size_t>(buffer.size(), file_size_ - offset);
    std::size_t actual = 0U;
    result = storage_.read_file_at(target_path_, offset, buffer.data(), count,
                                   actual) &&
             actual == count;
    if (result) {
      mbedtls_sha256_update(&check, buffer.data(), actual);
      offset += static_cast<std::uint32_t>(actual);
    }
  }
  std::uint8_t digest[32]{};
  if (result) {
    mbedtls_sha256_finish(&check, digest);
    result = std::memcmp(digest, expected_sha_, sizeof(digest)) == 0;
  }
  mbedtls_sha256_free(&check);
  return result;
}

void AssetUsbUpdater::finish_file() noexcept {
  if (!file_open_ || window_used_ != 0U || file_offset_ != file_size_) {
    abort("incomplete_file");
    return;
  }
  std::uint8_t digest[32]{};
  mbedtls_sha256_finish(&file_sha_, digest);
  if (std::memcmp(digest, expected_sha_, sizeof(digest)) != 0 ||
      !verify_stored_file()) {
    abort("file_hash");
    return;
  }
  manifest_received_ = manifest_received_ ||
                       std::strcmp(relative_, "asset_manifest.json") == 0;
  if (!manifest_received_) {
    std::strcpy(previous_relative_, relative_);
  }
  accepted_bytes_ += file_size_;
  ++accepted_file_count_;
  file_open_ = false;
  relative_[0] = '\0';
  target_path_[0] = '\0';
  Serial.print("ASSET_FILE_OK\n");
}

void AssetUsbUpdater::commit_bundle() noexcept {
  if (file_open_ || !manifest_received_ ||
      accepted_file_count_ != expected_file_count_ ||
      accepted_bytes_ != expected_total_bytes_ || !commit_transaction()) {
    abort("commit");
    return;
  }
  active_ = false;
  phase_ = AssetUpdatePhase::completed;
  preparation_ = AssetUpdatePreparationStep::none;
  Serial.print("ASSET_COMPLETE\n");
  HMI_LOG_INFO("[asset-usb] complete profile=%s files=%u bytes=%lu\r\n",
               profile_id_, static_cast<unsigned>(accepted_file_count_),
               static_cast<unsigned long>(accepted_bytes_));
}

void AssetUsbUpdater::send_status() noexcept {
  Serial.printf("ASSET_PROFILE %s\n", profile_id_);
  if (storage_available_) {
    Serial.printf("ASSET_SPACE %llu\n",
                  static_cast<unsigned long long>(storage_.free_bytes()));
  } else {
    Serial.print("ASSET_STORAGE unavailable\n");
  }
  Serial.print("ASSET_STATUS_END\n");
}

void AssetUsbUpdater::abort(const char* reason) noexcept {
  file_open_ = false;
  active_ = false;
  phase_ = AssetUpdatePhase::failed;
  preparation_ = AssetUpdatePreparationStep::none;
  recovering_ = false;
  window_used_ = 0U;
  window_chunks_ = 0U;
  recover_transaction();
  Serial.printf("ASSET_ERROR %s\n", reason == nullptr ? "unknown" : reason);
  HMI_LOG_ERROR("[asset-usb] failed reason=%s\r\n",
                reason == nullptr ? "unknown" : reason);
}

}  // namespace hmi::boards::jc1060p470c::internal
