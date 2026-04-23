#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common_ui.h"
#include "middle_sensor.h"
#include <stdio.h>

typedef struct {
  esp_err_t shtc3_init_ret;
  float temp_c;
  float hum_rh;
} shtc3_state_t;

typedef struct {
  example_ui_t *base;
} SHTC3_UI;

static SHTC3_UI ui;
static shtc3_state_t shtc3_state;
static lv_obj_t *hum_val_label;
static char hum_val_text[96];

static void shtc3_ui_init(void) {
  /* =======================
   * 1. Create Base UI
   * ======================= */
  ui.base = common_ui_get();
  common_ui_init();
  example_ui_t *b = ui.base;
  lv_obj_t *scr = lv_scr_act();

  /* =======================
   * 2. Create Extra Label (Humidity Value)
   * ======================= */
  hum_val_label = lv_label_create(scr);
  lv_obj_set_style_text_align(b->line1_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_text_color(hum_val_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(hum_val_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_width(hum_val_label, lv_pct(100));
  lv_obj_set_style_text_align(hum_val_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(hum_val_label, b->line4_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

}

static void shtc3_ui_apply(const shtc3_state_t *st) {
  /* =======================
   * 1. Title & Status Color
   * ======================= */
  example_ui_t *b = ui.base;
  snprintf(b->line1_text, sizeof(b->line1_text), "SHTC3");
  lv_label_set_text(b->line1_label, b->line1_text);
  switch (st->shtc3_init_ret) {
    case ESP_OK:
      lv_obj_set_style_text_color(b->line1_label, lv_color_hex(0x00FF00), 0);
      break;
    case ESP_ERR_NOT_SUPPORTED:
      lv_obj_set_style_text_color(b->line1_label, lv_color_hex(0x808080), 0);
      break;
    default:
      lv_obj_set_style_text_color(b->line1_label, lv_color_hex(0xFF0000), 0);
      break;
  }
  /* =======================
   * 2. Format Temperature/Humidity Text
   * ======================= */
  snprintf(b->line2_text, sizeof(b->line2_text), "Temp");
  snprintf(b->line3_text, sizeof(b->line3_text), "%.2fC", st->temp_c);
  snprintf(b->line4_text, sizeof(b->line4_text), "Hum");
  snprintf(hum_val_text, sizeof(hum_val_text), "%.2f%%", st->hum_rh);
  lv_label_set_text(b->line2_label, b->line2_text);
  lv_label_set_text(b->line3_label, b->line3_text);
  lv_label_set_text(b->line4_label, b->line4_text);
  lv_label_set_text(hum_val_label, hum_val_text);
}

static void shtc3_data_update(lv_timer_t *t) {
  /* =======================
   * 1. Read Sensor & Refresh UI
   * ======================= */
  float temp = 0.0f;
  float hum = 0.0f;
  esp_err_t r = middle_read_shtc3(&temp, &hum);
  if (r == ESP_OK) {
    shtc3_state.temp_c = temp;
    shtc3_state.hum_rh = hum;
  }
  shtc3_state.shtc3_init_ret = r;
  shtc3_ui_apply(&shtc3_state);
}

void shtc3_start(void) {
  /* =======================
   * 1. Start Display & UI
   * ======================= */
  bool locked = bsp_display_lock(0);
  if (locked) {
    shtc3_ui_init();
    bsp_display_unlock();
  }
  /* =======================
   * 2. Init Sensor & Timer
   * ======================= */
  shtc3_state.shtc3_init_ret = middle_init_shtc3();
  locked = bsp_display_lock(0);
  if (locked) {
    ui_create_timer(500, shtc3_data_update);
    bsp_display_unlock();
  }

  /* =======================
   * 3. Idle Loop
   * ======================= */
  while (true){
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
