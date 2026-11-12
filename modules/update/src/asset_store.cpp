#include "hmi/update/asset_store.hpp"

namespace hmi::update {

bool generation_protected(std::string_view generation,
                          std::string_view active,
                          std::string_view previous,
                          std::string_view pending) noexcept {
  return !generation.empty() &&
         (generation == active || generation == previous ||
          generation == pending);
}

Error visit_stale_generations(const std::string_view* generations,
                              std::size_t count, std::string_view active,
                              std::string_view previous,
                              std::string_view pending,
                              std::size_t scan_budget,
                              AssetEntryVisitor visitor,
                              void* context) noexcept {
  if ((count > 0U && generations == nullptr) || visitor == nullptr ||
      scan_budget == 0U || count > scan_budget) {
    return Error::invalid_argument;
  }
  for (std::size_t index = 0U; index < count; ++index) {
    const auto generation = generations[index];
    if (!valid_uuid(generation)) {
      return Error::invalid_argument;
    }
    const bool is_active = generation == active;
    const bool is_previous = generation == previous;
    const bool is_pending = generation == pending;
    if (!generation_protected(generation, active, previous, pending) &&
        !visitor(context, generation, is_active, is_previous, is_pending)) {
      return Error::storage;
    }
  }
  return Error::none;
}

}  // namespace hmi::update
