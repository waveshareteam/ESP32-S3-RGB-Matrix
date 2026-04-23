#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common_ui.h"
#include "middle_rtc.h"
#include <stdio.h>

typedef struct {
  esp_err_t rtc_init_ret;
  esp_err_t rtc_read_ret;
  pcf85063a_datetime_t rtc_time;
} rtc_state_t;

typedef struct {
  common_ui *base;
  lv_obj_t *m1_val_label;
  lv_obj_t *m2_val_label;
} RTC_UI;

static RTC_UI ui;
static rtc_state_t rtc_state;

// the line5 label is used to show the rtc time in the matrix
static lv_obj_t *line5_label;
static char line5_text[96];

static void rtc_ui_init(void) {
  /* =======================
   * 1. RTC Init Time
   * ======================= */
  rtc_state.rtc_time = (pcf85063a_datetime_t){
      .year = 2026,
      .month = 4,
      .day = 12,
      .hour = 21,
      .min = 9,
      .sec = 56,
  };

  /* =======================
   * 2. Create Base UI
   * ======================= */
  ui.base = common_ui_get();
  common_ui_init();
  example_ui_t *b = ui.base;
  lv_obj_t *scr = lv_scr_act();

  /* =======================
   * 3. Create Line 5
   * ======================= */
  line5_label = lv_label_create(scr);
  lv_obj_set_width(line5_label, lv_pct(100));
  lv_obj_set_style_text_color(line5_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_align(line5_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(line5_label, LV_FONT_DEFAULT, 0);

  /* =======================
   * 4. line1（Align top left）
   * ======================= */
  lv_obj_set_width(b->line1_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line1_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(b->line1_label, LV_ALIGN_TOP_MID, 0, 0);

  /* =======================
   * 5. line2（Align center）
   * ======================= */
  lv_obj_clear_flag(b->line2_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line2_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line2_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line2_label, b->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0,  0);

  /* =======================
   * 6. line3（Align center）
   * ======================= */
  lv_obj_clear_flag(b->line3_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line3_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line3_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line3_label, b->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0,  2);

  /* =======================
   * 7. line4（Align center）
   * ======================= */
  lv_obj_clear_flag(b->line4_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line4_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line4_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line4_label, b->line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0,  0);

  /* =======================
   * 8. line5（Align center）
   * ======================= */
  lv_obj_set_width(line5_label, lv_pct(100));
  lv_obj_set_style_text_align(line5_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(line5_label, b->line4_label, LV_ALIGN_OUT_BOTTOM_MID, 0,  0);
}

static void rtc_ui_apply(const rtc_state_t *st) {

  example_ui_t *b = ui.base;
  /* =========================
   * 1. RTC Title
   * ========================= */
  snprintf(b->line1_text, sizeof(b->line1_text), "RTC");
  lv_label_set_text(b->line1_label, b->line1_text);
  lv_color_t title_color;
  switch (st->rtc_init_ret) {
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

  /* =========================
   * 2. Data Validity Check
   * ========================= */
  const bool ok = (st->rtc_init_ret == ESP_OK) && (st->rtc_read_ret == ESP_OK);

  /* =========================
   * 3. Normal Display Time
   * ========================= */
  if (ok) {
    snprintf(b->line2_text, sizeof(b->line2_text), "%04u",
             (unsigned)st->rtc_time.year);

    snprintf(b->line3_text, sizeof(b->line3_text), "%02u-%02u",
             (unsigned)st->rtc_time.month, (unsigned)st->rtc_time.day);

    snprintf(b->line4_text, sizeof(b->line4_text), "%02u:%02u:%02u",
             (unsigned)st->rtc_time.hour, (unsigned)st->rtc_time.min,
             (unsigned)st->rtc_time.sec);

    snprintf(line5_text, sizeof(line5_text), "alarm:%u",
             (unsigned)(middle_rtc_alarm_seq() & 1U));
  }
  /* =========================
   * 4. Error Display Time
   * ========================= */
  else {
    snprintf(b->line2_text, sizeof(b->line2_text), "Error");

    snprintf(b->line3_text, sizeof(b->line3_text), "R:%d",
             (int)st->rtc_read_ret);

    b->line4_text[0] = '\0';
    line5_text[0] = '\0';
  }

  /* =========================
   * 5. UI Refresh
   * ========================= */
  lv_label_set_text(b->line2_label, b->line2_text);
  lv_label_set_text(b->line3_label, b->line3_text);
  lv_label_set_text(b->line4_label, b->line4_text);
  lv_label_set_text(line5_label, line5_text);
}

static void rtc_data_update(lv_timer_t *t) {
  pcf85063a_datetime_t now;
  esp_err_t r = middle_rtc_get_time(&now);
  rtc_state.rtc_read_ret = r;
  if (r == ESP_OK)
    rtc_state.rtc_time = now;
  rtc_ui_apply(&rtc_state);
}

void rtc_start(void) {
  bool locked = bsp_display_lock(0);
  if (locked) {
    rtc_ui_init();
    bsp_display_unlock();
  }
  middle_rtc_init();
  middle_rtc_set_time(rtc_state.rtc_time);
  middle_rtc_alarm(3);
  locked = bsp_display_lock(0);
  if (locked) {
    ui_create_timer(1, rtc_data_update);
    bsp_display_unlock();
  }
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
