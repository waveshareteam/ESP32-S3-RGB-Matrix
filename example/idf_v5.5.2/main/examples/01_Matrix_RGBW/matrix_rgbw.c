#include "bsp/display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdbool.h>

static const char *TAG = "RGBW";

typedef struct {
  const char *name;
  uint32_t hex_color;
} rgbw_color_t;

static const rgbw_color_t rgbw_colors[] = {{"Red", 0xFF0000},
                                           {"Green", 0x00FF00},
                                           {"Blue", 0x0000FF},
                                           {"White", 0xFFFFFF}};

static void rgbw_set_color(uint32_t hex_color) {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(hex_color), 0);
}

void rgbw_start(void) {
  /* =======================
   * 1. Basic Parameters
   * ======================= */
  int color_index = 0;
  const int num_colors = sizeof(rgbw_colors) / sizeof(rgbw_colors[0]);

  /* =======================
   * 2. Init Screen
   * ======================= */
  bool locked = bsp_display_lock(1000);
  if (locked) {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    bsp_display_unlock();
  }
  /* =======================
   * 3. Loop Colors
   * ======================= */
  while (true) {
    locked = bsp_display_lock(1000);
    if (locked) {
      rgbw_set_color(rgbw_colors[color_index].hex_color);
      bsp_display_unlock();
    }
    ESP_LOGI(TAG, "Current color: %s", rgbw_colors[color_index].name);
    vTaskDelay(pdMS_TO_TICKS(2000));
    color_index++;
    if (color_index >= num_colors) {
      color_index = 0;
    }
  }
}
