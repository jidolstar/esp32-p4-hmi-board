#pragma once

#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "hmi/ui/components.hpp"

namespace hmi::samples::suite {

inline hmi::ui::OverlayText overlay_text(hmi::i18n::Locale locale,
                                         generated::TextId id) noexcept {
  return {generated::text(locale, id).data(), generated::fixed_font(generated::typography(id))};
}

}  // namespace hmi::samples::suite
