#pragma once

#include <cstdint>

namespace hmi::settings {

/**
 * 밝기·음량 저장과 실제 장치 적용을 공통 model에 연결하는 비소유 port다.
 *
 * context와 callback이 참조하는 adapter는 앱과 LevelSettings보다 오래 살아야 한다.
 * 장치 handle, NVS API와 WAV 경로 정책은 이 계약 밖의 host adapter가 소유한다.
 */
struct LevelSettingsPort {
  void* context = nullptr;
  bool (*load)(void*, std::uint8_t&, std::uint8_t&, bool&, bool&) = nullptr;
  bool (*save_brightness)(void*, std::uint8_t) = nullptr;
  bool (*save_volume)(void*, std::uint8_t) = nullptr;
  bool (*apply_brightness)(void*, std::uint8_t) = nullptr;
  bool (*apply_volume)(void*, std::uint8_t) = nullptr;
  bool (*play_preview)(void*) = nullptr;
  bool (*stop_preview)(void*) = nullptr;
};

}  // namespace hmi::settings
