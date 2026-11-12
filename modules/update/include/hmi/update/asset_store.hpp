#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hmi/update/types.hpp"

namespace hmi::update {

using AssetEntryVisitor = bool (*)(void*, std::string_view generation,
                                   bool active, bool previous, bool pending);

/** 실제 directory 삭제 없이 보호해야 할 generation을 판정하는 domain-neutral 정책이다. */
[[nodiscard]] bool generation_protected(
    std::string_view generation, std::string_view active,
    std::string_view previous, std::string_view pending) noexcept;

/** 최대 scan budget 안에서만 stale generation 후보를 visitor에 전달한다. */
[[nodiscard]] Error visit_stale_generations(
    const std::string_view* generations, std::size_t count,
    std::string_view active, std::string_view previous,
    std::string_view pending, std::size_t scan_budget,
    AssetEntryVisitor visitor, void* context) noexcept;

}  // namespace hmi::update
