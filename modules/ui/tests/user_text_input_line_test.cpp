#include <cassert>
#include <cstring>
#include <string>

#include <lvgl.h>

#include "hmi/ui/font_repository.hpp"
#include "hmi/ui/user_text_input_line.hpp"
#include "pointer_test_driver.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

lv_font_t* load_font(void*, const char* path, std::uint32_t,
                     std::uint32_t) noexcept {
  return path[0] == 'c' ? const_cast<lv_font_t*>(&lv_font_montserrat_24)
                        : const_cast<lv_font_t*>(&lv_font_montserrat_16);
}

void free_font(void*, lv_font_t*) noexcept {}

struct SelectionCapture {
  std::size_t index = 999U;
  std::size_t calls = 0U;
};

void capture_selection(void* context, std::size_t index) noexcept {
  auto* capture = static_cast<SelectionCapture*>(context);
  capture->index = index;
  ++capture->calls;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace hmi::ui;
  static constexpr FontAsset assets[] = {{"common.bin"}, {"hangul.bin"}};
  static constexpr FontRange ranges[] = {
      {0x20U, 0x7EU, 0U}, {0x25A1U, 0x25A1U, 0U}, {0xAC00U, 0xD7A3U, 1U}};
  static constexpr FontManifestView manifest{assets, 2U, ranges, 3U, 0x25A1U};

  lv_init();
  hmi::platform::windows::SdlLvglPort port;
  assert(port.begin("user text input line", false));
  FontRepository repository;
  assert(repository.initialize(manifest, {nullptr, load_font, free_font}));

  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);
  lv_obj_t* parent = lv_obj_create(screen);
  assert(parent != nullptr);
  lv_obj_set_size(parent, 240, 80);
  lv_obj_set_pos(parent, 10, 10);

  UserTextInputLine line;
  assert(line.create(parent, repository, 200, 44));
  lv_obj_set_pos(line.object(), 20, 20);
  line.set_colors(lv_color_hex(0xEAF0FF), lv_color_hex(0x45A3FF));
  assert(line.render("A한B", 1U) == UserTextInputLineStatus::ok);
  assert(line.display_codepoint_count() == 3U);
  assert(line.nearest_cursor_index(-10) == 0U);
  assert(line.nearest_cursor_index(1000) == 3U);
  const lv_coord_t middle_caret = line.caret_x();
  assert(middle_caret > 0);

  // 조합 preview는 한 codepoint 경계와 밑줄을 추가하고 caret를 그 뒤에 둔다.
  assert(line.render("AB", 1U, 0xD55CU) == UserTextInputLineStatus::ok);
  assert(line.display_codepoint_count() == 3U);
  assert(line.composition_visible());
  assert(line.caret_x() > middle_caret);

  // 잘못된 cursor 요청은 기존 표시와 geometry를 보존한다.
  const auto count_before = line.display_codepoint_count();
  const auto caret_before = line.caret_x();
  assert(line.render("AB", 3U) == UserTextInputLineStatus::cursor_out_of_range);
  assert(line.display_codepoint_count() == count_before);
  assert(line.caret_x() == caret_before);
  const char invalid[] = {static_cast<char>(0xE3), static_cast<char>(0x81), '\0'};
  assert(line.render(invalid, 0U) == UserTextInputLineStatus::invalid_utf8);
  assert(line.display_codepoint_count() == count_before);

  line.set_active(true);
  assert(line.caret_visible());
  if (argc >= 2) {
    const bool dark = argc < 3 || std::strcmp(argv[2], "light") != 0;
    lv_obj_set_style_bg_color(screen,
                              dark ? lv_color_hex(0x111827) : lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    line.set_colors(dark ? lv_color_hex(0xE5E7EB) : lv_color_hex(0x111827),
                    dark ? lv_color_hex(0x60A5FA) : lv_color_hex(0x2563EB));
    assert(line.render("AB", 1U, 'C') == UserTextInputLineStatus::ok);
    port.tick();
    assert(port.save_screenshot(argv[1]));
  }
  lv_tick_inc(UserTextInputLine::kBlinkPeriodMs + 1U);
  lv_timer_handler();
  assert(!line.caret_visible());
  line.reset_blink();
  assert(line.caret_visible());
  line.set_active(false);
  assert(!line.caret_visible());

  // 실제 pointer 경로는 가장 가까운 codepoint 경계만 callback한다.
  SelectionCapture capture;
  line.set_selection_handler(&capture_selection, &capture);
  line.set_active(true);
  lv_obj_update_layout(screen);
  lv_area_t area{};
  lv_obj_get_coords(line.object(), &area);
  hmi::platform::windows::testing::click_pointer(
      port, area.x1 + 5, (area.y1 + area.y2) / 2);
  assert(capture.calls == 1U);
  assert(capture.index == 0U);
  hmi::platform::windows::testing::click_pointer(
      port, area.x2 - 5, (area.y1 + area.y2) / 2);
  assert(capture.calls == 2U);
  assert(capture.index == line.display_codepoint_count());

  // 최대 길이에서도 caret가 viewport 안에 오도록 가로 scroll한다.
  const std::string long_text(UserTextInputLine::kMaxCommittedCodepoints, 'W');
  assert(line.render(long_text, long_text.size()) == UserTextInputLineStatus::ok);
  lv_obj_update_layout(screen);
  assert(line.scroll_x() > 0);

  // parent 삭제 시 timer와 callback 수명을 함께 끊고 destroy는 다시 호출해도 안전하다.
  lv_obj_del(parent);
  assert(line.object() == nullptr);
  lv_tick_inc(UserTextInputLine::kBlinkPeriodMs + 1U);
  lv_timer_handler();
  line.destroy();
  repository.reset();
  return 0;
}
