#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <lvgl.h>
#include <src/extra/libs/fsdrv/lv_fsdrv.h>

#include "hmi/text/keyboard_layout.hpp"
#include "pages/text_input/text_input_page.hpp"
#include "pointer_test_driver.hpp"
#include "sdl_lvgl_port.hpp"

namespace {

struct Harness {
  hmi::i18n::Locale locale = hmi::i18n::Locale::ko_kr;
};

void ignore(void*) noexcept {}

bool set_locale(void* context, hmi::i18n::Locale locale) noexcept {
  auto* harness = static_cast<Harness*>(context);
  harness->locale = locale;
  return true;
}

void* load_font(void*, const char* relative_path, std::uint32_t size_bytes,
                std::uint32_t crc32) noexcept {
  std::array<char, 320U> path{};
  const int written = std::snprintf(path.data(), path.size(),
                                    "A:samples/suite/%s", relative_path);
  if (written <= 0 || static_cast<std::size_t>(written) >= path.size()) {
    return nullptr;
  }
  return hmi::ui::load_binary_font_file(path.data(), size_bytes, crc32);
}

void free_font(void*, void* font) noexcept {
  lv_font_free(static_cast<lv_font_t*>(font));
}

std::size_t key_index(const hmi::samples::suite::TextInputPage& page,
                      std::uint32_t codepoint) {
  const auto snapshot = page.text_snapshot();
  const auto layout = hmi::text::keyboard_layout(snapshot.keyboard_mode, false);
  for (std::size_t index = 0U; index < layout.size; ++index) {
    if (layout.keys[index].codepoint == codepoint) {
      return index;
    }
  }
  return layout.size;
}

lv_obj_t* find_label(lv_obj_t* parent, const char* text) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (lv_obj_check_type(parent, &lv_label_class) &&
      std::strcmp(lv_label_get_text(parent), text) == 0) {
    return parent;
  }
  const std::uint32_t count = lv_obj_get_child_cnt(parent);
  for (std::uint32_t index = 0U; index < count; ++index) {
    if (lv_obj_t* found = find_label(lv_obj_get_child(parent, index), text)) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace

int main() {
  using hmi::i18n::Locale;
  using hmi::samples::suite::TextFieldId;
  using hmi::samples::suite::TextInputFlowState;
  using hmi::samples::suite::TextInputPage;
  using hmi::text::KeyboardMode;

  std::printf("sizes InputModel=%zu UserTextInputLine=%zu VirtualKeyboard=%zu TextInputPage=%zu\n",
              sizeof(hmi::text::InputModel), sizeof(hmi::ui::UserTextInputLine),
              sizeof(hmi::ui::VirtualKeyboard), sizeof(TextInputPage));

  lv_init();
  lv_fs_stdio_init();
  hmi::platform::windows::SdlLvglPort port;
  assert(port.begin("text input state", false));
  lv_obj_t* screen = lv_obj_create(nullptr);
  assert(screen != nullptr);
  lv_scr_load(screen);

  Harness harness{};
  hmi::ui::UiTheme theme;
  assert(theme.initialize());
  hmi::samples::suite::PageContext context;
  context.request_dashboard = &ignore;
  context.request_theme_toggle = &ignore;
  context.set_locale = &set_locale;
  context.locale = harness.locale;
  context.fonts = {nullptr, &load_font, &free_font};
  context.theme = &theme;
  context.user_data = &harness;

  TextInputPage page;
  assert(page.create(screen, context));
  assert(page.set_locale(Locale::en_us));
  assert(harness.locale == Locale::en_us);
  assert(page.text_snapshot().keyboard_mode == KeyboardMode::hangul);

  for (std::uint32_t codepoint : {0x314EU, 0x314FU, 0x3134U}) {
    const std::size_t index = key_index(page, codepoint);
    assert(index < 26U);
    assert(page.press_character(index));
  }
  assert(page.text_snapshot().composing);
  assert(page.text_snapshot().codepoints == 0U);
  port.tick();
  lv_obj_t* preview = find_label(lv_scr_act(), "한");
  assert(preview != nullptr);
  lv_area_t preview_area{};
  lv_obj_get_coords(preview, &preview_area);
  hmi::platform::windows::testing::click_pointer(
      port, preview_area.x2, (preview_area.y1 + preview_area.y2) / 2);
  assert(!page.text_snapshot().composing);
  assert(page.text_snapshot().codepoints == 1U);
  assert(page.text_snapshot().cursor == 1U);

  lv_obj_t* memo_placeholder = find_label(lv_scr_act(), "Enter a memo");
  assert(memo_placeholder != nullptr);
  lv_area_t memo_area{};
  lv_obj_get_coords(memo_placeholder, &memo_area);
  hmi::platform::windows::testing::click_pointer(
      port, (memo_area.x1 + memo_area.x2) / 2, (memo_area.y1 + memo_area.y2) / 2);
  assert(page.text_snapshot().active_field == TextFieldId::memo);
  assert(page.text_snapshot().cursor == 0U);
  assert(page.field_text(TextFieldId::name) == "한");

  assert(page.handle_utf8_input("AB"));
  assert(page.handle_key(hmi::profiles::AppKey::cursor_left));
  assert(page.insert_space());
  assert(page.text_snapshot().codepoints == 3U);
  assert(page.text_snapshot().cursor == 2U);
  assert(page.field_text(TextFieldId::memo) == "A B");
  assert(page.complete());
  assert(page.text_snapshot().active_field == TextFieldId::value);

  assert(page.set_keyboard_mode(KeyboardMode::number));
  assert(page.press_character(0U));
  assert(page.text_snapshot().codepoints == 1U);
  assert(page.field_text(TextFieldId::value) == "1");

  lv_obj_t* memo_first_glyph = find_label(lv_scr_act(), "A");
  assert(memo_first_glyph != nullptr);
  lv_area_t glyph_area{};
  lv_obj_get_coords(memo_first_glyph, &glyph_area);
  hmi::platform::windows::testing::click_pointer(
      port, glyph_area.x1, (glyph_area.y1 + glyph_area.y2) / 2);
  assert(page.text_snapshot().active_field == TextFieldId::memo);
  assert(page.text_snapshot().cursor == 0U);
  assert(page.complete());
  assert(page.text_snapshot().active_field == TextFieldId::value);
  assert(page.text_snapshot().locale == Locale::en_us);
  assert(page.complete());
  assert(page.text_snapshot().flow == TextInputFlowState::review);
  assert(page.edit_review());
  assert(page.handle_utf8_input("2"));
  assert(page.complete());
  assert(page.confirm_review());
  assert(page.text_snapshot().flow == TextInputFlowState::confirmed);
  assert(page.field_text(TextFieldId::value) == "12");

  // widget/theme만 재생성해도 page가 소유한 입력값과 flow는 유지된다.
  page.destroy();
  assert(theme.set_mode(hmi::ui::ThemeMode::kLight));
  context.theme = &theme;
  context.locale = Locale::en_us;
  assert(page.create(screen, context));
  assert(page.text_snapshot().flow == TextInputFlowState::confirmed);
  assert(page.field_text(TextFieldId::name) == "한");
  assert(page.field_text(TextFieldId::memo) == "A B");
  assert(page.field_text(TextFieldId::value) == "12");

  page.clear_all();
  assert(page.text_snapshot().codepoints == 0U);
  assert(page.text_snapshot().active_field == TextFieldId::name);
  assert(page.text_snapshot().flow == TextInputFlowState::editing);
  assert(page.complete());
  assert(page.complete());
  assert(page.complete());
  assert(page.text_snapshot().flow == TextInputFlowState::review);
  assert(page.edit_review());
  page.destroy();
  theme.reset();
  return 0;
}

