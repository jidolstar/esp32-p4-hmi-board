#pragma once

#include <cstdint>
#include <string_view>

#include "hmi/update/ports.hpp"

namespace hmi::update {

struct ManifestBinding {
  std::string_view installation_id{};
  std::string_view device_id{};
  Target target = Target::invalid;
  std::int64_t now_epoch = 0;
  bool allow_expired_recovery = false;
};

/**
 * 서명된 descriptor를 strict JSON으로 검증한다.
 *
 * descriptor/key/signature view는 호출 동안만 빌린다. 실패 시 output을 변경하지 않으며 성공한
 * 경우에만 bounded Manifest를 복사한다.
 */
[[nodiscard]] Error decode_manifest(std::string_view descriptor,
                                    std::string_view key_id,
                                    std::string_view signature,
                                    const ManifestBinding& binding,
                                    CryptoPort crypto,
                                    Manifest& output) noexcept;

}  // namespace hmi::update
