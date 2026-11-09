#pragma once

#include <cstddef>
#include <cstdint>

#include <mbedtls/sha256.h>

#include "asset_update_status.hpp"

namespace hmi::boards::jc1060p470c::internal {

class SdStorage;

/**
 * USB CDC로 받은 현재 profile 자산 묶음을 SD staging에 기록하고 교체한다.
 *
 * SdStorage는 Board가 소유하며 updater보다 오래 산다. updater는 SD mount 실패 중에도
 * CDC handshake에 진단 오류를 반환하지만 파일 수신은 시작하지 않는다. 설정된
 * profile root 밖의 경로를 받지 않고, file별 SHA-256 검증과 journal 복구가 끝난
 * 뒤에만 active directory를 바꾼다. UI 객체와 product 상태는 알지 않는다.
 */
class AssetUsbUpdater {
 public:
  explicit AssetUsbUpdater(SdStorage& storage) noexcept;
  AssetUsbUpdater(const AssetUsbUpdater&) = delete;
  AssetUsbUpdater& operator=(const AssetUsbUpdater&) = delete;
  ~AssetUsbUpdater();

  bool begin(const char* profile_id, const char* active_root) noexcept;
  void tick() noexcept;
  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] AssetUpdateStatus status() const noexcept;

 private:
  static constexpr std::size_t kChunkBytes = 96U;
  static constexpr std::size_t kWindowChunks = 16U;
  static constexpr std::size_t kWindowBytes = kChunkBytes * kWindowChunks;
  static constexpr std::uint32_t kTimeoutMs = 30000U;
  static constexpr std::uint32_t kPreparationTimeoutMs = 300000U;

  void handle_line() noexcept;
  void begin_preparation() noexcept;
  void cancel_preparation() noexcept;
  void begin_bundle() noexcept;
  void begin_file() noexcept;
  void append_data() noexcept;
  void rewind_file() noexcept;
  void finish_file() noexcept;
  void commit_bundle() noexcept;
  void send_status() noexcept;
  void request_retry() noexcept;
  void abort(const char* reason) noexcept;
  bool flush_window() noexcept;
  bool recover_transaction() noexcept;
  bool begin_transaction() noexcept;
  bool commit_transaction() noexcept;
  bool recompute_file_sha(std::uint32_t bytes) noexcept;
  bool verify_stored_file() noexcept;
  bool valid_relative_path(const char* path) const noexcept;
  bool make_transaction_paths(const char* active_root) noexcept;
  static bool decode_sha(const char* text, std::uint8_t* output) noexcept;

  SdStorage& storage_;
  mbedtls_sha256_context file_sha_{};
  char profile_id_[32]{};
  char active_root_[96]{};
  char incoming_root_[112]{};
  char backup_root_[112]{};
  char journal_path_[112]{};
  char line_[240]{};
  char relative_[120]{};
  char previous_relative_[120]{};
  char target_path_[232]{};
  char expected_sha_text_[65]{};
  std::uint8_t expected_sha_[32]{};
  std::uint8_t window_[kWindowBytes]{};
  std::size_t line_length_ = 0U;
  std::size_t window_used_ = 0U;
  std::uint32_t file_size_ = 0U;
  std::uint32_t file_offset_ = 0U;
  std::uint32_t expected_total_bytes_ = 0U;
  std::uint32_t accepted_bytes_ = 0U;
  std::uint32_t last_input_ms_ = 0U;
  std::uint16_t expected_file_count_ = 0U;
  std::uint16_t accepted_file_count_ = 0U;
  std::uint8_t window_chunks_ = 0U;
  bool enabled_ = false;
  bool storage_available_ = false;
  bool active_ = false;
  bool file_open_ = false;
  bool manifest_received_ = false;
  bool overflow_ = false;
  bool recovering_ = false;
  AssetUpdatePhase phase_ = AssetUpdatePhase::idle;
  AssetUpdatePreparationStep preparation_ = AssetUpdatePreparationStep::none;
};

static_assert(sizeof(AssetUsbUpdater) <= 3072U,
              "AssetUsbUpdater must keep a bounded USB window and path state");

}  // namespace hmi::boards::jc1060p470c::internal
