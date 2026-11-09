#include <cassert>

#include <lvgl.h>

#include "hmi/ui/font_repository.hpp"
#include "hmi/ui/user_text_label.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

lv_font_t* load_font(void*, const char* path, std::uint32_t,
                     std::uint32_t) noexcept {
  return path[0] == 'c' ? const_cast<lv_font_t*>(&lv_font_montserrat_24)
                        : const_cast<lv_font_t*>(&lv_font_montserrat_16);
}

void free_font(void*, lv_font_t*) noexcept {}

}  // namespace

int main() {
  using namespace hmi::ui;
  static constexpr FontAsset assets[] = {{"common.bin"}, {"hangul.bin"}};
  static constexpr FontRange ranges[] = {
      {0x20U, 0x7EU, 0U}, {0x25A1U, 0x25A1U, 0U}, {0xAC00U, 0xD7A3U, 1U}};
  static constexpr FontManifestView manifest{assets, 2U, ranges, 3U, 0x25A1U};

  lv_init();
  hmi::platform::windows::SdlLvglPort port;
  assert(port.begin("user text label", false));
  FontRepository repository;
  assert(repository.initialize(manifest, {nullptr, load_font, free_font}));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  UserTextLabel label;
  assert(label.create(screen, repository));
  lv_scr_load(screen);
  assert(label.set_text("ABC") == UserTextStatus::ok);
  assert(label.segment_count() == 1U);
  assert(label.set_text("A한🙂") == UserTextStatus::missing_glyph);
  assert(label.segment_count() == 3U);
  port.tick();

  const char invalid[] = {static_cast<char>(0xE3), static_cast<char>(0x81), '\0'};
  assert(label.set_text(invalid) == UserTextStatus::invalid_utf8);
  assert(label.segment_count() == 3U);

  label.destroy();
  repository.reset();
  return 0;
}

