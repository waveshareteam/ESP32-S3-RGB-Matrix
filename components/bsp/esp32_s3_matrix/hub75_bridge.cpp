#include <sdkconfig.h>
#include "hub75.h"

static Hub75Driver *s_driver = nullptr;

static Hub75Config make_config() {
  Hub75Config cfg{};
  cfg.panel_width = CONFIG_HUB75_PANEL_WIDTH;
  cfg.panel_height = CONFIG_HUB75_PANEL_HEIGHT;

  cfg.scan_pattern = (
#if defined(CONFIG_HUB75_SCAN_1_32)
      Hub75ScanPattern::SCAN_1_32
#elif defined(CONFIG_HUB75_SCAN_1_20)
      Hub75ScanPattern::SCAN_1_20
#elif defined(CONFIG_HUB75_SCAN_1_16)
      Hub75ScanPattern::SCAN_1_16
#else
      Hub75ScanPattern::SCAN_1_8
#endif
  );

  cfg.scan_wiring = (
#if defined(CONFIG_HUB75_WIRING_FOUR_SCAN_64PX)
      Hub75ScanWiring::FOUR_SCAN_64PX_HIGH
#elif defined(CONFIG_HUB75_WIRING_FOUR_SCAN_32PX)
      Hub75ScanWiring::FOUR_SCAN_32PX_HIGH
#elif defined(CONFIG_HUB75_WIRING_FOUR_SCAN_16PX)
      Hub75ScanWiring::FOUR_SCAN_16PX_HIGH
#else
      Hub75ScanWiring::STANDARD_TWO_SCAN
#endif
  );

  cfg.shift_driver = (
#if defined(CONFIG_HUB75_DRIVER_FM6126A)
      Hub75ShiftDriver::FM6126A
#elif defined(CONFIG_HUB75_DRIVER_FM6124)
      Hub75ShiftDriver::FM6124
#elif defined(CONFIG_HUB75_DRIVER_MBI5124)
      Hub75ShiftDriver::MBI5124
#elif defined(CONFIG_HUB75_DRIVER_DP3246)
      Hub75ShiftDriver::DP3246
#else
      Hub75ShiftDriver::GENERIC
#endif
  );

  cfg.layout_rows = CONFIG_HUB75_LAYOUT_ROWS;
  cfg.layout_cols = CONFIG_HUB75_LAYOUT_COLS;

  cfg.layout = (
#if defined(CONFIG_HUB75_LAYOUT_TOP_LEFT_DOWN)
      Hub75PanelLayout::TOP_LEFT_DOWN
#elif defined(CONFIG_HUB75_LAYOUT_TOP_RIGHT_DOWN)
      Hub75PanelLayout::TOP_RIGHT_DOWN
#elif defined(CONFIG_HUB75_LAYOUT_BOTTOM_LEFT_UP)
      Hub75PanelLayout::BOTTOM_LEFT_UP
#elif defined(CONFIG_HUB75_LAYOUT_BOTTOM_RIGHT_UP)
      Hub75PanelLayout::BOTTOM_RIGHT_UP
#elif defined(CONFIG_HUB75_LAYOUT_TOP_LEFT_DOWN_ZIGZAG)
      Hub75PanelLayout::TOP_LEFT_DOWN_ZIGZAG
#elif defined(CONFIG_HUB75_LAYOUT_TOP_RIGHT_DOWN_ZIGZAG)
      Hub75PanelLayout::TOP_RIGHT_DOWN_ZIGZAG
#elif defined(CONFIG_HUB75_LAYOUT_BOTTOM_LEFT_UP_ZIGZAG)
      Hub75PanelLayout::BOTTOM_LEFT_UP_ZIGZAG
#elif defined(CONFIG_HUB75_LAYOUT_BOTTOM_RIGHT_UP_ZIGZAG)
      Hub75PanelLayout::BOTTOM_RIGHT_UP_ZIGZAG
#else
      Hub75PanelLayout::HORIZONTAL
#endif
  );

  cfg.rotation = (
#if defined(CONFIG_HUB75_ROTATE_90)
      Hub75Rotation::ROTATE_90
#elif defined(CONFIG_HUB75_ROTATE_180)
      Hub75Rotation::ROTATE_180
#elif defined(CONFIG_HUB75_ROTATE_270)
      Hub75Rotation::ROTATE_270
#else
      Hub75Rotation::ROTATE_0
#endif
  );

  Hub75Pins pins{};
  pins.r1 = (int8_t)CONFIG_HUB75_PIN_R1;
  pins.g1 = (int8_t)CONFIG_HUB75_PIN_G1;
  pins.b1 = (int8_t)CONFIG_HUB75_PIN_B1;
  pins.r2 = (int8_t)CONFIG_HUB75_PIN_R2;
  pins.g2 = (int8_t)CONFIG_HUB75_PIN_G2;
  pins.b2 = (int8_t)CONFIG_HUB75_PIN_B2;
  pins.a = (int8_t)CONFIG_HUB75_PIN_A;
  pins.b = (int8_t)CONFIG_HUB75_PIN_B;
  pins.c = (int8_t)CONFIG_HUB75_PIN_C;
  pins.d = (int8_t)CONFIG_HUB75_PIN_D;
  pins.e = (int8_t)CONFIG_HUB75_PIN_E;
  pins.lat = (int8_t)CONFIG_HUB75_PIN_LAT;
  pins.oe = (int8_t)CONFIG_HUB75_PIN_OE;
  pins.clk = (int8_t)CONFIG_HUB75_PIN_CLK;
  cfg.pins = pins;

  cfg.output_clock_speed = (
#if defined(CONFIG_HUB75_CLK_8MHZ)
      Hub75ClockSpeed::HZ_8M
#elif defined(CONFIG_HUB75_CLK_10MHZ)
      Hub75ClockSpeed::HZ_10M
#elif defined(CONFIG_HUB75_CLK_16MHZ)
      Hub75ClockSpeed::HZ_16M
#else
      Hub75ClockSpeed::HZ_20M
#endif
  );
  cfg.min_refresh_rate = CONFIG_HUB75_MIN_REFRESH_RATE;
  cfg.latch_blanking = CONFIG_HUB75_LATCH_BLANKING;

  cfg.double_buffer =
#if defined(CONFIG_HUB75_DOUBLE_BUFFER)
      true;
#else
      false;
#endif

  cfg.clk_phase_inverted =
#if defined(CONFIG_HUB75_CLK_PHASE_INVERTED)
      true;
#else
      false;
#endif

  cfg.brightness = CONFIG_HUB75_BRIGHTNESS;

  return cfg;
}

extern "C" bool hub75_bridge_init(void) {
  if (s_driver) return true;
  Hub75Config cfg = make_config();
  s_driver = new Hub75Driver(cfg);
  return s_driver && s_driver->begin();
}

extern "C" void hub75_bridge_draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *buffer, bool big_endian) {
  if (!s_driver) return;
  s_driver->draw_pixels(x, y, w, h, buffer, Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, big_endian);
}

extern "C" void hub75_bridge_deinit(void) {
  if (!s_driver) return;
  delete s_driver;
  s_driver = nullptr;
}
