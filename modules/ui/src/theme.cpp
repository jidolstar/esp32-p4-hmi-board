#include "hmi/ui/theme.hpp"

namespace hmi::ui {
namespace {

struct Palette {
  std::uint32_t background;
  std::uint32_t header;
  std::uint32_t surface;
  std::uint32_t elevated;
  std::uint32_t border;
  std::uint32_t primary;
  std::uint32_t primary_pressed;
  std::uint32_t accent;
  std::uint32_t text;
  std::uint32_t muted;
  std::uint32_t success;
  std::uint32_t scrim;
};

constexpr Palette kDarkPalette{
    0x07111FU, 0x0B1B30U, 0x10243CU, 0x17314FU, 0x2A4968U, 0x6D5EF8U,
    0x5748E5U, 0x22D3EEU, 0xF7FAFFU, 0x9FB0C8U, 0x0F8F7DU, 0x020617U};
// 라이트 모드는 흰 surface 사이에서 계층이 보이도록 background,
// elevated, border에 단계적인 명도 차를 둔다.
constexpr Palette kLightPalette{
    0xE7EDF5U, 0xFFFFFFU, 0xFFFFFFU, 0xF1F5F9U, 0xC4D1DEU, 0x5B5CE2U,
    0x4748C9U, 0x0891B2U, 0x162033U, 0x5F6B7AU, 0x087F6DU, 0x162033U};

lv_color_t color(std::uint32_t value) noexcept { return lv_color_hex(value); }

void set_panel(lv_style_t &style, std::uint32_t background,
               std::uint32_t border, lv_coord_t radius) noexcept {
  lv_style_set_bg_color(&style, color(background));
  lv_style_set_bg_opa(&style, LV_OPA_COVER);
  lv_style_set_border_color(&style, color(border));
  lv_style_set_border_width(&style, 1);
  lv_style_set_radius(&style, radius);
}

void set_button(lv_style_t &style, std::uint32_t background,
                std::uint32_t border) noexcept {
  set_panel(style, background, border, 8);
  // 상태별 외곽 두께가 달라 보이지 않도록 공통 버튼은 그림자와 outline을 쓰지 않는다.
  lv_style_set_shadow_width(&style, 0);
  lv_style_set_shadow_opa(&style, LV_OPA_TRANSP);
  lv_style_set_outline_width(&style, 0);
}

void set_text(lv_style_t &style, const lv_font_t *font,
              std::uint32_t value) noexcept {
  lv_style_set_text_font(&style, font);
  lv_style_set_text_color(&style, color(value));
}

}  // namespace

lv_style_t &UiTheme::style(StyleId id) noexcept {
  return styles_[static_cast<std::size_t>(id)];
}

bool UiTheme::initialize(ThemeMode mode) noexcept {
  if (initialized_) {
    return false;
  }
  mode_ = mode;
  for (lv_style_t &entry : styles_) {
    lv_style_init(&entry);
  }
  initialized_ = true;
  configure();
  return true;
}

bool UiTheme::set_mode(ThemeMode mode) noexcept {
  if (!initialized_) {
    return initialize(mode);
  }
  if (mode_ == mode) {
    return true;
  }
  for (lv_style_t &entry : styles_) {
    lv_style_reset(&entry);
    lv_style_init(&entry);
  }
  mode_ = mode;
  configure();
  return true;
}

void UiTheme::configure() noexcept {
  const Palette &palette = mode_ == ThemeMode::kDark ? kDarkPalette
                                                      : kLightPalette;

  lv_style_t &screen = style(StyleId::kScreen);
  lv_style_set_bg_color(&screen, color(palette.background));
  lv_style_set_bg_opa(&screen, LV_OPA_COVER);
  lv_style_set_border_width(&screen, 0);
  lv_style_set_radius(&screen, 0);

  lv_style_t &header = style(StyleId::kHeader);
  set_panel(header, palette.header, palette.border, 18);
  lv_style_set_shadow_color(&header, color(palette.background));
  lv_style_set_shadow_width(&header, 22);
  lv_style_set_shadow_opa(&header, mode_ == ThemeMode::kDark ? LV_OPA_30
                                                             : LV_OPA_20);
  lv_style_set_shadow_ofs_y(&header, 8);

  lv_style_t &surface = style(StyleId::kSurface);
  set_panel(surface, palette.surface, palette.border, 18);

  lv_style_t &card = style(StyleId::kCard);
  set_panel(card, palette.elevated, palette.border, 16);
  lv_style_set_shadow_color(&card, color(palette.background));
  lv_style_set_shadow_width(&card, 18);
  lv_style_set_shadow_opa(&card, mode_ == ThemeMode::kDark ? LV_OPA_30
                                                           : LV_OPA_20);
  lv_style_set_shadow_ofs_y(&card, 7);

  lv_style_t &card_pressed = style(StyleId::kCardPressed);
  lv_style_set_bg_color(&card_pressed, color(palette.primary));
  lv_style_set_border_color(&card_pressed, color(palette.accent));

  lv_style_t &primary = style(StyleId::kButtonPrimary);
  set_button(primary, palette.primary, palette.primary);

  lv_style_t &primary_pressed = style(StyleId::kButtonPrimaryPressed);
  lv_style_set_bg_color(&primary_pressed, color(palette.primary_pressed));
  lv_style_set_shadow_width(&primary_pressed, 0);
  lv_style_set_translate_y(&primary_pressed, 2);

  lv_style_t &secondary = style(StyleId::kButtonSecondary);
  set_button(secondary, palette.elevated, palette.border);

  lv_style_t &secondary_pressed = style(StyleId::kButtonSecondaryPressed);
  lv_style_set_bg_color(&secondary_pressed, color(palette.border));
  lv_style_set_translate_y(&secondary_pressed, 2);

  lv_style_t &ghost = style(StyleId::kButtonGhost);
  lv_style_set_bg_opa(&ghost, LV_OPA_TRANSP);
  lv_style_set_border_color(&ghost, color(palette.border));
  lv_style_set_border_width(&ghost, 1);
  lv_style_set_radius(&ghost, 8);
  lv_style_set_shadow_width(&ghost, 0);
  lv_style_set_shadow_opa(&ghost, LV_OPA_TRANSP);
  lv_style_set_outline_width(&ghost, 0);

  lv_style_t &ghost_pressed = style(StyleId::kButtonGhostPressed);
  lv_style_set_bg_color(&ghost_pressed, color(palette.elevated));
  lv_style_set_bg_opa(&ghost_pressed, LV_OPA_COVER);

  set_text(style(StyleId::kTextDisplay), &lv_font_montserrat_32, palette.text);
  set_text(style(StyleId::kTextTitle), &lv_font_montserrat_24, palette.text);
  set_text(style(StyleId::kTextBody), &lv_font_montserrat_16, palette.text);
  set_text(style(StyleId::kTextMuted), &lv_font_montserrat_16, palette.muted);
  set_text(style(StyleId::kTextInverse), &lv_font_montserrat_16, 0xFFFFFFU);
  set_text(style(StyleId::kTextAccent), &lv_font_montserrat_16, palette.accent);

  lv_style_t &chip = style(StyleId::kChip);
  set_panel(chip, palette.elevated, palette.border, 999);

  lv_style_t &status = style(StyleId::kStatus);
  set_panel(status, palette.elevated, palette.border, 12);
  lv_style_set_border_side(&status, LV_BORDER_SIDE_LEFT);
  lv_style_set_border_width(&status, 4);
  lv_style_set_border_color(&status, color(palette.accent));

  set_panel(style(StyleId::kListItem), palette.elevated, palette.border, 12);

  lv_style_t &toast = style(StyleId::kToast);
  set_panel(toast, palette.success, palette.success, 14);
  lv_style_set_shadow_color(&toast, color(palette.background));
  lv_style_set_shadow_width(&toast, 22);
  lv_style_set_shadow_opa(&toast, LV_OPA_50);
  lv_style_set_shadow_ofs_y(&toast, 8);

  lv_style_t &scrim = style(StyleId::kScrim);
  lv_style_set_bg_color(&scrim, color(palette.scrim));
  lv_style_set_bg_opa(&scrim, LV_OPA_70);
  lv_style_set_border_width(&scrim, 0);
  lv_style_set_radius(&scrim, 0);

  lv_style_t &dialog = style(StyleId::kDialog);
  set_panel(dialog, palette.surface, palette.border, 20);
  lv_style_set_shadow_color(&dialog, color(palette.scrim));
  lv_style_set_shadow_width(&dialog, 30);
  lv_style_set_shadow_opa(&dialog, LV_OPA_50);

  lv_style_t &transparent = style(StyleId::kTransparent);
  lv_style_set_bg_opa(&transparent, LV_OPA_TRANSP);
  lv_style_set_border_width(&transparent, 0);
  lv_style_set_radius(&transparent, 0);
  lv_style_set_shadow_width(&transparent, 0);
}

ThemeMode UiTheme::opposite_mode() const noexcept {
  return mode_ == ThemeMode::kDark ? ThemeMode::kLight : ThemeMode::kDark;
}

lv_color_t UiTheme::text_color() const noexcept {
  return color(mode_ == ThemeMode::kDark ? kDarkPalette.text : kLightPalette.text);
}

lv_color_t UiTheme::accent_color() const noexcept {
  return color(mode_ == ThemeMode::kDark ? kDarkPalette.accent
                                         : kLightPalette.accent);
}

void UiTheme::apply_screen(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kScreen), 0);
}

void UiTheme::apply_header(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kHeader), 0);
}

void UiTheme::apply_surface(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kSurface), 0);
}

void UiTheme::apply_card(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kCard), 0);
  lv_obj_add_style(object, &style(StyleId::kCardPressed), LV_STATE_PRESSED);
}

void UiTheme::apply_button(lv_obj_t *object, ButtonVariant variant) noexcept {
  StyleId normal = StyleId::kButtonPrimary;
  StyleId pressed = StyleId::kButtonPrimaryPressed;
  if (variant == ButtonVariant::kSecondary) {
    normal = StyleId::kButtonSecondary;
    pressed = StyleId::kButtonSecondaryPressed;
  } else if (variant == ButtonVariant::kGhost) {
    normal = StyleId::kButtonGhost;
    pressed = StyleId::kButtonGhostPressed;
  }
  lv_obj_add_style(object, &style(normal), 0);
  lv_obj_add_style(object, &style(pressed), LV_STATE_PRESSED);
}

void UiTheme::apply_label(lv_obj_t *label, TextRole role) noexcept {
  StyleId id = StyleId::kTextBody;
  switch (role) {
    case TextRole::kDisplay: id = StyleId::kTextDisplay; break;
    case TextRole::kTitle: id = StyleId::kTextTitle; break;
    case TextRole::kBody: id = StyleId::kTextBody; break;
    case TextRole::kMuted: id = StyleId::kTextMuted; break;
    case TextRole::kInverse: id = StyleId::kTextInverse; break;
    case TextRole::kAccent: id = StyleId::kTextAccent; break;
  }
  lv_obj_add_style(label, &style(id), 0);
}

void UiTheme::apply_chip(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kChip), 0);
}

void UiTheme::apply_status(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kStatus), 0);
}

void UiTheme::apply_list_item(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kListItem), 0);
}

void UiTheme::apply_toast(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kToast), 0);
}

void UiTheme::apply_scrim(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kScrim), 0);
}

void UiTheme::apply_dialog(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kDialog), 0);
}

void UiTheme::apply_transparent(lv_obj_t *object) noexcept {
  lv_obj_add_style(object, &style(StyleId::kTransparent), 0);
}

void UiTheme::reset() noexcept {
  if (!initialized_) {
    return;
  }
  for (lv_style_t &entry : styles_) {
    lv_style_reset(&entry);
  }
  initialized_ = false;
  mode_ = ThemeMode::kDark;
}

UiTheme::~UiTheme() { reset(); }

}  // namespace hmi::ui
