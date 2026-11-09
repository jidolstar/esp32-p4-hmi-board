#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include <lvgl.h>
#include <src/extra/libs/fsdrv/lv_fsdrv.h>

#include "hmi/ui/font_repository.hpp"
#include "hmi/ui/user_text_label.hpp"
#include "generated/fixed_fonts.hpp"
#include "generated/text_catalog.hpp"
#include "generated/user_font_manifest.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

struct StdioFontLoader {
  const char* prefix = "samples/suite/";
};

lv_font_t* load_font(void* context, const char* relative_path,
                     std::uint32_t size_bytes,
                     std::uint32_t crc32) noexcept {
  auto* loader = static_cast<StdioFontLoader*>(context);
  std::array<char, 256U> path{};
  const int written = std::snprintf(path.data(), path.size(), "A:%s%s",
                                    loader->prefix, relative_path);
  if (written <= 0 || static_cast<std::size_t>(written) >= path.size()) {
    return nullptr;
  }
  return hmi::ui::load_binary_font_file(path.data(), size_bytes, crc32);
}

void free_font(void*, lv_font_t* font) noexcept { lv_font_free(font); }

lv_obj_t* fixed_label(lv_obj_t* parent, const char* text,
                      hmi::samples::suite::generated::TypographyRole role,
                      lv_color_t color) {
  namespace generated = hmi::samples::suite::generated;
  lv_obj_t* label = lv_label_create(parent);
  assert(label != nullptr);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, generated::fixed_font(role), 0);
  lv_obj_set_style_text_color(label, color, 0);
  return label;
}

}  // namespace

int main(int argc, char** argv) {
  namespace generated = hmi::samples::suite::generated;
  using hmi::i18n::Locale;
  using hmi::ui::FontRepository;
  using hmi::ui::UserTextLabel;
  using hmi::ui::UserTextStatus;

  lv_init();
  lv_fs_stdio_init();
  lv_font_glyph_dsc_t value_glyph{};
  assert(lv_font_get_glyph_dsc(
      generated::fixed_font(generated::TypographyRole::page_title),
      &value_glyph, static_cast<std::uint32_t>('0'), 0U));
  hmi::platform::windows::SdlLvglPort port;
  assert(port.begin("font render", argc > 1));

  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x071722U), 0);
  lv_obj_set_style_pad_all(screen, 36, 0);
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(screen, 22, 0);

  fixed_label(screen,
              generated::text(Locale::ko_kr, generated::TextId::page_text_input_title).data(),
              generated::TypographyRole::page_title, lv_color_hex(0xF4F7FFU));
  fixed_label(screen,
              generated::text(Locale::ko_kr, generated::TextId::page_text_input_description).data(),
              generated::TypographyRole::body, lv_color_hex(0xA9BED7U));
  fixed_label(screen,
              generated::text(Locale::en_us, generated::TextId::page_text_input_description).data(),
              generated::TypographyRole::body, lv_color_hex(0xA9BED7U));

  StdioFontLoader loader{};
  const auto& common_asset = generated::kUserFontAssets[0];
  std::array<char, 256U> common_path{};
  const int common_written = std::snprintf(
      common_path.data(), common_path.size(), "A:%s%s", loader.prefix,
      common_asset.relative_path);
  assert(common_written > 0 &&
         static_cast<std::size_t>(common_written) < common_path.size());
  assert(hmi::ui::load_binary_font_file(
             common_path.data(), common_asset.size_bytes,
             common_asset.crc32 ^ 0x1U) == nullptr);
  lv_font_t* verified_common_font = hmi::ui::load_binary_font_file(
      common_path.data(), common_asset.size_bytes, common_asset.crc32);
  assert(verified_common_font != nullptr);
  lv_font_free(verified_common_font);

  FontRepository repository;
  assert(repository.initialize(generated::kUserFontManifest,
                               {&loader, load_font, free_font}));
  UserTextLabel mixed;
  assert(mixed.create(screen, repository));
  const UserTextStatus mixed_status = mixed.set_text("한글 ABC 123, 값");
  assert(mixed_status == UserTextStatus::ok);
  lv_obj_set_style_text_color(mixed.object(), lv_color_hex(0x38D9FFU), 0);

  UserTextLabel replacement;
  assert(replacement.create(screen, repository));
  assert(replacement.set_text("지원하지 않는 문자 🙂 → □") ==
         UserTextStatus::missing_glyph);
  lv_obj_set_style_text_color(replacement.object(), lv_color_hex(0xFFCF66U), 0);

  lv_scr_load(screen);
  for (int frame = 0; frame < 12; ++frame) {
    port.tick();
  }
  assert(repository.loaded_count() >= 2U);
  if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
    assert(port.save_screenshot(argv[2]));
  }

  replacement.destroy();
  mixed.destroy();
  repository.reset();
  return 0;
}

