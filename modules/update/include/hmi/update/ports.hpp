#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hmi/update/types.hpp"

namespace hmi::update {

/** 서명과 streaming SHA-256 구현을 platform 또는 crypto adapter가 제공하는 비소유 계약이다. */
struct CryptoPort {
  void* context = nullptr;
  bool (*verify)(void*, std::string_view key_id, const std::uint8_t*,
                 std::size_t, std::string_view signature) = nullptr;
  bool (*digest_begin)(void*) = nullptr;
  bool (*digest_update)(void*, const std::uint8_t*, std::size_t) = nullptr;
  bool (*digest_finish)(void*, char (&)[65]) = nullptr;
  void (*digest_abort)(void*) = nullptr;
};

/** 원격 artifact를 offset부터 순차 읽는 transport 계약이다. endpoint와 인증 정책은 adapter가 소유한다. */
struct DownloadPort {
  void* context = nullptr;
  Error (*open)(void*, std::string_view path, std::uint64_t offset,
                std::uint64_t expected_size) = nullptr;
  Error (*read)(void*, std::uint8_t*, std::size_t, std::size_t&, bool&) = nullptr;
  void (*close)(void*) = nullptr;
};

/** 미완료 artifact와 commit된 artifact의 수명·배타권을 저장소 adapter가 소유하는 계약이다. */
struct StagingPort {
  void* context = nullptr;
  Error (*partial_size)(void*, std::string_view id, std::uint64_t&) = nullptr;
  Error (*read_partial)(void*, std::string_view id, std::uint64_t,
                        std::uint8_t*, std::size_t, std::size_t&) = nullptr;
  Error (*append_partial)(void*, std::string_view id, const std::uint8_t*,
                          std::size_t) = nullptr;
  Error (*truncate_partial)(void*, std::string_view id,
                            std::uint64_t) = nullptr;
  Error (*commit)(void*, std::string_view id) = nullptr;
  Error (*remove_partial)(void*, std::string_view id) = nullptr;
};

enum class DownloadIntent : std::uint8_t { continue_download, pause, cancel };

/** UI나 worker owner가 취소·일시정지와 진행률만 전달하는 비소유 callback 묶음이다. */
struct DownloadControl {
  void* context = nullptr;
  DownloadIntent (*intent)(void*) = nullptr;
  void (*progress)(void*, std::uint64_t completed,
                   std::uint64_t total) = nullptr;
};

inline constexpr std::size_t kDurableRecordBytes = 160U;

/** A/B record slot의 실제 NVS·file 구현을 숨기는 고정 크기 저장 계약이다. */
struct DurableRecordPort {
  void* context = nullptr;
  Error (*read)(void*, std::uint8_t slot, std::uint8_t*, std::size_t,
                std::size_t&) = nullptr;
  Error (*write)(void*, std::uint8_t slot, const std::uint8_t*,
                 std::size_t) = nullptr;
  Error (*flush)(void*) = nullptr;
};

/** 검증된 firmware bytes를 실제 partition writer에 전달하는 비소유 계약이다. */
struct FirmwareWriterPort {
  void* context = nullptr;
  Error (*begin)(void*, std::string_view installation_id,
                 std::uint64_t expected_size) = nullptr;
  Error (*write)(void*, std::uint64_t offset, const std::uint8_t*,
                 std::size_t) = nullptr;
  Error (*finish)(void*, std::string_view expected_sha256) = nullptr;
  void (*abort)(void*) = nullptr;
};

/** pending image 선택, 실행 확인, rollback, 재시작을 platform adapter가 제공하는 계약이다. */
struct BootControlPort {
  void* context = nullptr;
  Error (*mark_pending)(void*, std::string_view installation_id) = nullptr;
  Error (*confirm_running)(void*) = nullptr;
  Error (*request_rollback)(void*) = nullptr;
  Error (*restart)(void*) = nullptr;
};

static_assert(sizeof(FirmwareWriterPort) <= 5U * sizeof(void*));
static_assert(sizeof(BootControlPort) <= 5U * sizeof(void*));

}  // namespace hmi::update
