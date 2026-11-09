#include <cassert>
#include <cstddef>

#include <lvgl.h>

#include "hmi/ui/font_repository.hpp"
#include "hmi/ui/theme.hpp"
#include "hmi/ui/virtual_keyboard.hpp"
#include "pointer_test_driver.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

lv_font_t* load_font(void*, const char*, std::uint32_t,
                     std::uint32_t) noexcept {
  return const_cast<lv_font_t*>(&lv_font_montserrat_24);
}

void free_font(void*, lv_font_t*) noexcept {}

struct EventCapture {
  hmi::ui::VirtualKeyEvent event{};
  std::size_t calls = 0U;
};

void capture_event(void* context, const hmi::ui::VirtualKeyEvent& event) noexcept {
  auto* capture = static_cast<EventCapture*>(context);
  capture->event = event;
  ++capture->calls;
}

std::size_t find_key(const hmi::ui::VirtualKeyboard& keyboard,
                     hmi::ui::VirtualKeyAction action,
                     std::uint32_t codepoint = 0U) {
  for (std::size_t index = 0U; index < keyboard.key_count(); ++index) {
    const auto event = keyboard.key_event(index);
    if (event.action == action &&
        (codepoint == 0U || event.codepoint == codepoint)) {
      return index;
    }
  }
  return keyboard.key_count();
}

void click_key(hmi::platform::windows::SdlLvglPort& port,
               const hmi::ui::VirtualKeyboard& keyboard, std::size_t index) {
  assert(index < keyboard.key_count());
  lv_obj_t* button = keyboard.key_object(index);
  assert(button != nullptr);
  lv_area_t area{};
  lv_obj_get_coords(button, &area);
  hmi::platform::windows::testing::click_pointer(
      port, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2);
}

}  // namespace

int main(int argc, char** argv) {
  using namespace hmi::ui;
  static constexpr FontAsset assets[] = {{"user.bin"}};
  static constexpr FontRange ranges[] = {
      {0x20U, 0x7EU, 0U}, {0x25A1U, 0x25A1U, 0U}, {0x3131U, 0x3163U, 0U}};
  static constexpr FontManifestView manifest{assets, 1U, ranges, 3U, 0x25A1U};
  static constexpr VirtualKeyboardCaptions captions{
      "SHIFT", "Symbols", "KO/EN", "Space", "Backspace", "Next"};

  lv_init();
  hmi::platform::windows::SdlLvglPort port;
  assert(port.begin("virtual keyboard", false));
  FontRepository repository;
  assert(repository.initialize(manifest, {nullptr, load_font, free_font}));
  UiTheme theme;
  assert(theme.initialize(ThemeMode::kDark));

  lv_obj_t* screen = lv_obj_create(nullptr);
  lv_obj_t* parent = screen == nullptr ? nullptr : lv_obj_create(screen);
  assert(screen != nullptr && parent != nullptr);
  theme.apply_screen(screen);
  theme.apply_surface(parent);
  lv_obj_set_size(parent, 620, 410);
  lv_obj_set_pos(parent, 20, 20);
  lv_obj_set_style_pad_all(parent, 10, 0);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_scr_load(screen);

  VirtualKeyboard keyboard;
  assert(keyboard.create(parent, repository, theme, &lv_font_montserrat_16,
                         600, 390));
  EventCapture capture;
  keyboard.set_event_handler(&capture_event, &capture);
  assert(keyboard.render(hmi::text::KeyboardMode::hangul, false, captions));
  assert(keyboard.row_count() == 5U);
  assert(keyboard.row_height() >= VirtualKeyboard::kMinimumKeyHeight);
  assert(keyboard.key_count() == 44U);
  assert(keyboard.key_event(0U).codepoint == '1');
  assert(keyboard.key_event(9U).codepoint == '0');
  assert(keyboard.key_event(10U).action == VirtualKeyAction::layout_character);
  assert(keyboard.key_event(10U).codepoint == 0x3142U);  // ㅂ
  assert(keyboard.key_row(10U) == 1U);
  assert(keyboard.key_event(19U).codepoint == 0x3154U);  // ㅔ
  assert(keyboard.key_event(20U).codepoint == 0x3141U);  // ㅁ

  const std::size_t shift = find_key(keyboard, VirtualKeyAction::shift);
  const std::size_t submit = find_key(keyboard, VirtualKeyAction::submit);
  const std::size_t language = find_key(keyboard, VirtualKeyAction::language);
  const std::size_t space = find_key(keyboard, VirtualKeyAction::space);
  const std::size_t comma = find_key(keyboard, VirtualKeyAction::literal_character, ',');
  assert(shift < keyboard.key_count());
  assert(submit < keyboard.key_count());
  assert(language < keyboard.key_count());
  assert(space < keyboard.key_count());
  assert(comma < keyboard.key_count());
  assert(keyboard.key_row(shift) == 3U);
  assert(keyboard.key_row(space) == 4U);

  lv_obj_update_layout(screen);
  lv_area_t keyboard_area{};
  lv_area_t first_number_key_area{};
  lv_area_t last_number_key_area{};
  lv_area_t first_letter_key_area{};
  lv_area_t rightmost_hangul_key_area{};
  lv_area_t submit_area{};
  lv_obj_get_coords(keyboard.object(), &keyboard_area);
  lv_obj_get_coords(keyboard.key_object(0U), &first_number_key_area);
  lv_obj_get_coords(keyboard.key_object(9U), &last_number_key_area);
  lv_obj_get_coords(keyboard.key_object(10U), &first_letter_key_area);
  lv_obj_get_coords(keyboard.key_object(19U), &rightmost_hangul_key_area);
  lv_obj_get_coords(keyboard.key_object(submit), &submit_area);
  assert(first_number_key_area.x1 == first_letter_key_area.x1);
  assert(last_number_key_area.x2 == rightmost_hangul_key_area.x2);
  assert(rightmost_hangul_key_area.x2 <= keyboard_area.x2);
  assert(submit_area.x2 <= keyboard_area.x2);
  assert(lv_obj_get_height(keyboard.key_object(space)) >=
         VirtualKeyboard::kMinimumKeyHeight);
  assert(lv_obj_get_width(keyboard.key_object(space)) >
         2 * lv_obj_get_width(keyboard.key_object(comma)));

  click_key(port, keyboard, shift);
  assert(capture.calls == 1U);
  assert(capture.event.action == VirtualKeyAction::shift);
  click_key(port, keyboard, space);
  assert(capture.calls == 2U);
  assert(capture.event.action == VirtualKeyAction::space);
  click_key(port, keyboard, language);
  assert(capture.calls == 3U);
  assert(capture.event.action == VirtualKeyAction::language);

  assert(keyboard.render(hmi::text::KeyboardMode::latin, true, captions));
  assert(keyboard.key_event(10U).codepoint == 'Q');
  assert(find_key(keyboard, VirtualKeyAction::shift) < keyboard.key_count());
  assert(keyboard.render(hmi::text::KeyboardMode::symbol, false, captions));
  assert(keyboard.key_count() == 33U);
  assert(find_key(keyboard, VirtualKeyAction::shift) == keyboard.key_count());
  const std::size_t symbol_toggle = find_key(keyboard, VirtualKeyAction::symbol);
  assert(symbol_toggle < keyboard.key_count());

  if (argc == 2) {
    lv_obj_invalidate(keyboard.object());
    lv_refr_now(nullptr);
    assert(port.save_screenshot(argv[1]));
  }

  lv_obj_del(parent);
  assert(keyboard.object() == nullptr);
  keyboard.destroy();
  repository.reset();
  theme.reset();
  return 0;
}
