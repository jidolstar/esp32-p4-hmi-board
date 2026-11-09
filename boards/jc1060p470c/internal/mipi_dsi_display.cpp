#include "internal/mipi_dsi_display.hpp"

#include <Arduino.h>
#include <driver/gpio.h>

#include "config.hpp"
#include "log.hpp"

namespace hmi::boards::jc1060p470c::internal {
namespace {

constexpr gpio_num_t kPanelResetPin = GPIO_NUM_27;
constexpr gpio_num_t kBacklightPin = GPIO_NUM_23;
constexpr std::uint32_t kBacklightPwmFrequency = 20000U;
constexpr std::uint8_t kBacklightPwmResolution = 8U;
constexpr std::uint8_t kMinBacklightLevel = 1U;
constexpr std::uint8_t kMaxBacklightLevel = 10U;

// 사람의 체감 밝기에 맞춰 저단계 간격을 촘촘하게 둔 1~10 PWM duty table이다.
constexpr std::uint8_t kBacklightDutyByLevel[] = {
    24U, 32U, 43U, 57U, 75U, 98U, 127U, 164U, 208U, 255U,
};

constexpr bool backlight_table_is_valid() {
  if (kBacklightDutyByLevel[0] == 0U ||
      kBacklightDutyByLevel[kMaxBacklightLevel - 1U] != 255U) {
    return false;
  }
  for (std::size_t index = 1U; index < kMaxBacklightLevel; ++index) {
    if (kBacklightDutyByLevel[index] <= kBacklightDutyByLevel[index - 1U]) {
      return false;
    }
  }
  return true;
}

static_assert(sizeof(kBacklightDutyByLevel) == kMaxBacklightLevel);
static_assert(backlight_table_is_valid());

struct InitCommand {
  std::uint8_t command;
  std::uint8_t data[14];
  std::uint8_t size;
  std::uint16_t delay_ms;
};

// 일부 출하분에 장착된 new panel revision용 JD9165 command table이다.
constexpr InitCommand kInitCommands[] = {
    {0x30, {0x00}, 1, 0}, {0xF7, {0x49, 0x61, 0x02, 0x00}, 4, 0},
    {0x30, {0x01}, 1, 0}, {0x04, {0x0C}, 1, 0},
    {0x05, {0x08}, 1, 0}, {0x0B, {0x11}, 1, 0},
    {0x20, {0x04}, 1, 0}, {0x1F, {0x05}, 1, 0},
    {0x23, {0x38}, 1, 0}, {0x28, {0x18}, 1, 0},
    {0x29, {0x29}, 1, 0}, {0x2A, {0x01}, 1, 0},
    {0x2B, {0x29}, 1, 0}, {0x2C, {0x01}, 1, 0},
    {0x30, {0x02}, 1, 0}, {0x00, {0x05}, 1, 0},
    {0x01, {0x22}, 1, 0}, {0x02, {0x08}, 1, 0},
    {0x03, {0x12}, 1, 0}, {0x04, {0x16}, 1, 0},
    {0x05, {0x64}, 1, 0}, {0x06, {0x00}, 1, 0},
    {0x07, {0x00}, 1, 0}, {0x08, {0x78}, 1, 0},
    {0x09, {0x00}, 1, 0}, {0x0A, {0x04}, 1, 0},
    {0x0B, {0x16, 0x17, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x07,
            0x07, 0x09}, 11, 0},
    {0x0C, {0x09, 0x1E, 0x1E, 0x1C, 0x1C, 0x0D, 0x0D, 0x0D, 0x0D,
            0x0D, 0x0D}, 11, 0},
    {0x0D, {0x0A, 0x05, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x06,
            0x06, 0x08}, 11, 0},
    {0x0E, {0x08, 0x1F, 0x1F, 0x1D, 0x1D, 0x0D, 0x0D, 0x0D, 0x0D,
            0x0D, 0x0D}, 11, 0},
    {0x0F, {0x0A, 0x05, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1D,
            0x1D, 0x1F}, 11, 0},
    {0x10, {0x1F, 0x08, 0x08, 0x06, 0x06, 0x0D, 0x0D, 0x0D, 0x0D,
            0x0D, 0x0D}, 11, 0},
    {0x11, {0x16, 0x17, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1C,
            0x1C, 0x1E}, 11, 0},
    {0x12, {0x1E, 0x09, 0x09, 0x07, 0x07, 0x0D, 0x0D, 0x0D, 0x0D,
            0x0D, 0x0D}, 11, 0},
    {0x13, {0x00, 0x00, 0x00, 0x00}, 4, 0},
    {0x14, {0x00, 0x00, 0x41, 0x41}, 4, 0},
    {0x15, {0x00, 0x00, 0x00, 0x00}, 4, 0},
    {0x17, {0x00}, 1, 0}, {0x18, {0x85}, 1, 0},
    {0x19, {0x06, 0x09}, 2, 0}, {0x1A, {0x05, 0x08}, 2, 0},
    {0x1B, {0x0A, 0x04}, 2, 0}, {0x26, {0x00}, 1, 0},
    {0x27, {0x00}, 1, 0}, {0x30, {0x06}, 1, 0},
    {0x12, {0x3F, 0x25, 0x27, 0x35, 0x1D, 0x1B, 0x1B, 0x1A, 0x18,
            0x0A, 0x2A, 0x21, 0x19, 0x30}, 14, 0},
    {0x13, {0x3F, 0x26, 0x27, 0x35, 0x1E, 0x1C, 0x1C, 0x1A, 0x18,
            0x0B, 0x2A, 0x21, 0x19, 0x30}, 14, 0},
    {0x30, {0x0A}, 1, 0}, {0x02, {0x4F}, 1, 0},
    {0x0B, {0x40}, 1, 0}, {0x30, {0x0D}, 1, 0},
    {0x0D, {0x04}, 1, 0}, {0x10, {0x05}, 1, 0},
    {0x11, {0x0C}, 1, 0}, {0x12, {0x05}, 1, 0},
    {0x13, {0x0C}, 1, 0}, {0x30, {0x00}, 1, 0},
    {0x3A, {0x55}, 1, 0}, {0x11, {0x00}, 1, 120},
    {0x29, {0x00}, 1, 20},
};

bool check(esp_err_t error, const char *step) {
  if (error == ESP_OK) {
    return true;
  }
  HMI_LOG_ERROR("[lcd] %s failed err=0x%x\r\n", step, error);
  return false;
}

}  // namespace

bool MipiDsiDisplay::begin() {
  set_backlight(false);

  const esp_ldo_channel_config_t ldo_config = {
      .chan_id = 3,
      .voltage_mv = 2500,
  };
  if (!check(esp_ldo_acquire_channel(&ldo_config, &phy_power_),
             "phy power")) {
    return false;
  }

  const esp_lcd_dsi_bus_config_t bus_config = {
      .bus_id = 0,
      .num_data_lanes = 2,
      .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
      .lane_bit_rate_mbps = 550,
  };
  if (!check(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus_), "DSI bus")) {
    return false;
  }

  const esp_lcd_dbi_io_config_t io_config = {
      .virtual_channel = 0,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  if (!check(esp_lcd_new_panel_io_dbi(dsi_bus_, &io_config, &command_io_),
             "DBI io")) {
    return false;
  }

  const esp_lcd_dpi_panel_config_t panel_config = {
      .virtual_channel = 0,
      .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
      .dpi_clock_freq_mhz = 52,
      .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
      .num_fbs = 2,
      .video_timing = {
          .h_size = config::kScreenWidth,
          .v_size = config::kScreenHeight,
          .hsync_pulse_width = 24,
          .hsync_back_porch = 136,
          .hsync_front_porch = 160,
          .vsync_pulse_width = 2,
          .vsync_back_porch = 21,
          .vsync_front_porch = 12,
      },
      .flags = {.use_dma2d = true},
  };
  if (!check(esp_lcd_new_panel_dpi(dsi_bus_, &panel_config, &panel_),
             "DPI panel")) {
    return false;
  }

  const gpio_config_t reset_config = {
      .pin_bit_mask = 1ULL << kPanelResetPin,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  if (!check(gpio_config(&reset_config), "reset gpio")) {
    return false;
  }
  gpio_set_level(kPanelResetPin, 1);
  delay(5);
  gpio_set_level(kPanelResetPin, 0);
  delay(10);
  gpio_set_level(kPanelResetPin, 1);
  delay(120);

  const std::uint8_t madctl = 0x00;
  if (!check(esp_lcd_panel_io_tx_param(command_io_, 0x36, &madctl, 1),
             "JD9165 MADCTL")) {
    return false;
  }

  for (const auto &entry : kInitCommands) {
    if (!check(esp_lcd_panel_io_tx_param(command_io_, entry.command,
                                         entry.data, entry.size),
               "JD9165 command")) {
      return false;
    }
    if (entry.delay_ms > 0) {
      delay(entry.delay_ms);
    }
  }

  if (!check(esp_lcd_panel_init(panel_), "panel init")) {
    return false;
  }
  if (!check(esp_lcd_dpi_panel_get_frame_buffer(
                 panel_, 2, &framebuffer_a_, &framebuffer_b_),
             "framebuffers")) {
    return false;
  }

  set_backlight(true);
  HMI_LOG_INFO("[lcd] init ok %ux%u fb1=%p fb2=%p\r\n",
               static_cast<unsigned>(config::kScreenWidth),
               static_cast<unsigned>(config::kScreenHeight), framebuffer_a_,
               framebuffer_b_);
  return true;
}

void MipiDsiDisplay::set_backlight(bool on) {
  if (backlight_pwm_attached_) {
    if (!ledcWrite(kBacklightPin, on ? backlight_duty_ : 0U)) {
      HMI_LOG_ERROR("[backlight] PWM on/off write failed\r\n");
    }
    return;
  }
  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, on ? HIGH : LOW);
}

bool MipiDsiDisplay::set_backlight_level(std::uint8_t level) {
  if (level < kMinBacklightLevel || level > kMaxBacklightLevel) {
    HMI_LOG_ERROR("[backlight] invalid level=%u\r\n",
                  static_cast<unsigned>(level));
    return false;
  }
  if (!backlight_pwm_attached_) {
    if (!ledcAttach(kBacklightPin, kBacklightPwmFrequency,
                    kBacklightPwmResolution)) {
      HMI_LOG_ERROR("[backlight] PWM attach failed pin=%u\r\n",
                    static_cast<unsigned>(kBacklightPin));
      return false;
    }
    backlight_pwm_attached_ = true;
  }
  const std::uint8_t duty =
      kBacklightDutyByLevel[level - kMinBacklightLevel];
  if (!ledcWrite(kBacklightPin, duty)) {
    HMI_LOG_ERROR("[backlight] PWM write failed level=%u duty=%u\r\n",
                  static_cast<unsigned>(level),
                  static_cast<unsigned>(duty));
    return false;
  }
  backlight_duty_ = duty;
  HMI_LOG_INFO("[backlight] level=%u duty=%u\r\n",
               static_cast<unsigned>(level), static_cast<unsigned>(duty));
  return true;
}

esp_lcd_panel_handle_t MipiDsiDisplay::panel_handle() const { return panel_; }

void *MipiDsiDisplay::framebuffer_a() const { return framebuffer_a_; }

void *MipiDsiDisplay::framebuffer_b() const { return framebuffer_b_; }

}  // namespace hmi::boards::jc1060p470c::internal
