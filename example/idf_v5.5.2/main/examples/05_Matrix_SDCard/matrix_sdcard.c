#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common_ui.h"
#include "middle_sdcard.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdint.h>

typedef struct {
  esp_err_t sd_init_ret;
  size_t sd_total_mb;
  size_t sd_free_mb;
  size_t sd_size_mb;
} sdcard_state_t;

typedef struct {
  example_ui_t *base;
} SDCard_UI;

static SDCard_UI ui;
static sdcard_state_t sd_state;

static void sdcard_ui_init(void) {
  /* =======================
   * 1. Create Base UI
   * ======================= */
  ui.base = common_ui_get();
  common_ui_init();
  example_ui_t *b = ui.base;

  /* =======================
   *  2. line1（Align top left）
   * ======================= */
  lv_obj_set_style_text_font(b->line1_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_width(b->line1_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line1_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(b->line1_label, LV_ALIGN_TOP_MID, 0, -1);

  /* =======================
   *  3. line2（Align center）
   * ======================= */
  lv_obj_clear_flag(b->line2_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(b->line2_label, &lv_font_unscii_16, 0);
  lv_obj_set_width(b->line2_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line2_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line2_label, b->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, -1);

  /* =======================
   *  4. line3（Align center）
   * ======================= */
  lv_obj_clear_flag(b->line3_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(b->line3_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_width(b->line3_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line3_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line3_label, b->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, -2);
  
  // hide line4
  lv_obj_add_flag(b->line4_label, LV_OBJ_FLAG_HIDDEN);
}

static void sdcard_ui_apply(const sdcard_state_t *st) {
  /* =======================
   * 1. Title & Status Color
   * ======================= */
  example_ui_t *b = ui.base;
  snprintf(b->line1_text, sizeof(b->line1_text), "SD");
  lv_label_set_text(b->line1_label, b->line1_text);
  lv_color_t title_color;
  switch (st->sd_init_ret) {
  case ESP_OK:
    title_color = lv_color_hex(0x00FF00);
    break;
  case ESP_ERR_NOT_SUPPORTED:
    title_color = lv_color_hex(0x808080);
    break;
  default:
    title_color = lv_color_hex(0xFF0000);
    break;
  }
  lv_obj_set_style_text_color(b->line1_label, title_color, 0);

  /* =======================
   * 2. Format Capacity Text
   * ======================= */
  snprintf(b->line2_text, sizeof(b->line2_text), "Mem");
  snprintf(b->line3_text, sizeof(b->line3_text), "---");
  if ((st->sd_init_ret == ESP_OK) && (st->sd_size_mb > 0)) {
    uint32_t mb = (uint32_t)st->sd_size_mb;
    uint32_t gb10 = (mb * 10U + 512U) / 1024U;
    unsigned gb_i = (unsigned)(gb10 / 10U);
    unsigned gb_f = (unsigned)(gb10 % 10U);
    snprintf(b->line3_text, sizeof(b->line3_text), "%u.%uG", gb_i, gb_f);
  }
  lv_label_set_text(b->line2_label, b->line2_text);
  lv_label_set_text(b->line3_label, b->line3_text);
}

static void sdcard_data_update(lv_timer_t *t) {
  /* =======================
   * 1. Read Capacity & Refresh UI
   * ======================= */
  size_t total_mb = 0;
  size_t free_mb = 0;
  esp_err_t r = middle_sdcard_get_capacity_mb(&total_mb, &free_mb);
  sd_state.sd_init_ret = r;
  if (r == ESP_OK) {
    sd_state.sd_total_mb = total_mb;
    sd_state.sd_free_mb = free_mb;
    sd_state.sd_size_mb = total_mb;
  } else {
    sd_state.sd_total_mb = 0;
    sd_state.sd_free_mb = 0;
    sd_state.sd_size_mb = 0;
  }
  sdcard_ui_apply(&sd_state);
}

void sdcard_start(void) {
  /* =======================
   * 1. Start Display & UI
   * ======================= */
  bool locked = bsp_display_lock(0);
  if (locked) {
    sdcard_ui_init();
    bsp_display_unlock();
  }

  /* =======================
   * 2. Init SD Card
   * ======================= */
  middle_sdcard_init();

  /* =======================
   * 3. Periodic Refresh
   * ======================= */
  locked = bsp_display_lock(0);
  if (locked) {
    ui_create_timer(1000, sdcard_data_update);
    bsp_display_unlock();
  }

  /* =======================
   * 4. Idle Loop
   * ======================= */
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
