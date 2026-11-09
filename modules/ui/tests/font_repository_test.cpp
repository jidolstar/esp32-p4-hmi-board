#include <cassert>
#include <cstdint>

#include <lvgl.h>

#include "hmi/ui/font_repository.hpp"

namespace {

struct LoaderState {
  unsigned loads = 0U;
  unsigned frees = 0U;
  bool fail_second = false;
};

lv_font_t* load_font(void* context, const char* path, std::uint32_t,
                     std::uint32_t) noexcept {
  auto* state = static_cast<LoaderState*>(context);
  assert(state != nullptr);
  ++state->loads;
  if (state->fail_second && path[0] == 'h') {
    return nullptr;
  }
  return path[0] == 'c' ? const_cast<lv_font_t*>(&lv_font_montserrat_24)
                        : const_cast<lv_font_t*>(&lv_font_montserrat_16);
}

void free_font(void* context, lv_font_t*) noexcept {
  auto* state = static_cast<LoaderState*>(context);
  assert(state != nullptr);
  ++state->frees;
}

}  // namespace

int main() {
  using hmi::ui::FontAsset;
  using hmi::ui::FontAssetLoader;
  using hmi::ui::FontManifestView;
  using hmi::ui::FontRange;
  using hmi::ui::FontRepository;
  using hmi::ui::FontResolveStatus;

  static constexpr FontAsset assets[] = {{"common.bin"}, {"hangul.bin"}};
  static constexpr FontRange ranges[] = {
      {0x20U, 0x7EU, 0U}, {0x25A1U, 0x25A1U, 0U}, {0xAC00U, 0xD7A3U, 1U}};
  static constexpr FontManifestView manifest{assets, 2U, ranges, 3U, 0x25A1U};

  LoaderState state{};
  FontRepository repository;
  assert(repository.initialize(manifest, {&state, load_font, free_font}));
  const auto latin = repository.resolve('A');
  assert(latin.status == FontResolveStatus::ok);
  assert(latin.font == &lv_font_montserrat_24);
  assert(repository.loaded_count() == 1U);
  assert(repository.resolve('B').font == latin.font);
  assert(state.loads == 1U);

  const auto hangul = repository.resolve(0xD55CU);
  assert(hangul.status == FontResolveStatus::ok);
  assert(hangul.font == &lv_font_montserrat_16);
  assert(repository.loaded_count() == 2U);
  assert(repository.resolve(0xAC01U).font == hangul.font);
  assert(state.loads == 2U);

  const auto missing = repository.resolve(0x1F642U);
  assert(missing.status == FontResolveStatus::missing_glyph);
  assert(missing.rendered_codepoint == 0x25A1U);
  assert(missing.font == latin.font);
  assert(state.loads == 2U);

  repository.reset();
  assert(state.frees == 2U);
  assert(!repository.initialized());

  state = {};
  state.fail_second = true;
  assert(repository.initialize(manifest, {&state, load_font, free_font}));
  assert(repository.resolve(0xAC00U).status == FontResolveStatus::load_failed);
  assert(repository.loaded_count() == 0U);

  static constexpr FontManifestView invalid{
      assets, FontRepository::kMaxCachedFonts + 1U, ranges, 3U, 0x25A1U};
  repository.reset();
  assert(!repository.initialize(invalid, {&state, load_font, free_font}));
  return 0;
}

