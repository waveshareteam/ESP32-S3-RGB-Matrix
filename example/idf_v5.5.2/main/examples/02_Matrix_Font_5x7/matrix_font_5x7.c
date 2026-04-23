
#include "matrix_font_5x7.h"

static const char *TAG = "Font_5x7";
typedef struct {
  example_ui_t *base;
} Font_5x7_UI;

static Font_5x7_UI ui;

static void font_5x7_ui_init(void) {
  /* =======================
   * 1. Init Common UI
   * ======================= */
  ui.base = common_ui_get();
  common_ui_init();
  example_ui_t *b = ui.base;

  /* =======================
   * 2. Enable Labels & Recolor
   * ======================= */
  lv_obj_clear_flag(b->line1_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(b->line2_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(b->line3_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(b->line4_label, LV_OBJ_FLAG_HIDDEN);

  lv_label_set_recolor(b->line1_label, true);
  lv_label_set_recolor(b->line2_label, true);
  lv_label_set_recolor(b->line3_label, true);
  lv_label_set_recolor(b->line4_label, true);

  /* =======================
   * 3. Set 5x7 Font & Spacing
   * ======================= */
  lv_obj_set_style_text_font(b->line1_label, &lv_font_5x7, 0);
  lv_obj_set_style_text_font(b->line2_label, &lv_font_5x7, 0);
  lv_obj_set_style_text_font(b->line3_label, &lv_font_5x7, 0);
  lv_obj_set_style_text_font(b->line4_label, &lv_font_5x7, 0);

  lv_obj_set_style_text_letter_space(b->line1_label, 1, 0);
  lv_obj_set_style_text_letter_space(b->line2_label, 1, 0);
  lv_obj_set_style_text_letter_space(b->line3_label, 1, 0);
  lv_obj_set_style_text_letter_space(b->line4_label, 1, 0);

  /* =======================
   * 4. Layout: 4 Lines
   * ======================= */
  lv_obj_align(b->line1_label, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_align(b->line2_label, LV_ALIGN_TOP_MID, 0, CONFIG_HUB75_PANEL_HEIGHT / 4);
  lv_obj_align(b->line3_label, LV_ALIGN_TOP_MID, 0, CONFIG_HUB75_PANEL_HEIGHT / 4 * 2);
  lv_obj_align(b->line4_label, LV_ALIGN_TOP_MID, 0, CONFIG_HUB75_PANEL_HEIGHT / 4 * 3);
}

static void font_5x7_ui_apply(void) {
  example_ui_t *b = ui.base;
  /* =======================
   * 1. Gradient Text Demo
   * ======================= */
  set_label_gradient_text(b->line1_label, "Welcome", 0xFF0000,
                          0xFFFF00); // red -> yellow
  set_label_gradient_text(b->line2_label, "Waveshare", 0xFFFF00,
                          0x00FF00); // yellow -> green
  set_label_gradient_text(b->line3_label, "LVGL HUB75", 0x00FF00,
                          0xFF00FF); // green -> purple
  set_label_gradient_text(b->line4_label, "RGB MATRIX", 0xFF00FF,
                          0xFFFF00); // purple -> yellow
}

void font_5x7_start(void) {
  /* =======================
   * 1. Start Display
   * ======================= */
  ESP_LOGI(TAG, "Matrix Font 5x7 start");
  /* =======================
   * 2. Init UI (LVGL Locked)
   * ======================= */
  bool locked = bsp_display_lock(0);
  if (locked) {
    font_5x7_ui_init();
    font_5x7_ui_apply();
    bsp_display_unlock();
  }

  /* =======================
   * 3. Idle Loop
   * ======================= */
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
