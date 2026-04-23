
#include "matrix_qmi.h"

typedef struct {
  esp_err_t qmi_init_ret;
  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;
} qmi_state_t;

typedef struct {
  example_ui_t *base;
} QMI_UI;

static QMI_UI ui;
static qmi_state_t qmi_state;

static lv_obj_t *qmi_line1_label;
static lv_obj_t *qmi_line2_label;
static lv_obj_t *qmi_line3_label;
static char qmi_line1_text[96];
static char qmi_line2_text[96];
static char qmi_line3_text[96];

static void qmi_ui_init(void) {
  /* =======================
   * 1. Create Base UI
   * ======================= */
  ui.base = common_ui_get();
  common_ui_init();
  example_ui_t *b = ui.base;
  lv_obj_t *scr = lv_scr_act();
  /* =======================
   * 2. Create Extra Labels (Gyro)
   * ======================= */
  qmi_line1_label = lv_label_create(scr);
  qmi_line2_label = lv_label_create(scr);
  qmi_line3_label = lv_label_create(scr);
  lv_obj_set_style_text_color(qmi_line1_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_color(qmi_line2_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_color(qmi_line3_label, lv_color_hex(0xFFFFFF), 0);
  /* =======================
   * 3. Layout & Fonts
   * ======================= */
  lv_obj_set_style_text_font(b->line1_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(b->line2_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(b->line3_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(b->line4_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(qmi_line1_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(qmi_line2_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(qmi_line3_label, LV_FONT_DEFAULT, 0);
  /* =======================
   * 4. line1（Align top left）
   * ======================= */
  lv_obj_set_width(b->line1_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line1_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(b->line1_label, 2, 0);
  lv_obj_align(b->line1_label, LV_ALIGN_TOP_LEFT, 0, 0);
  /* =======================
   * 5. line2（Align left）
   * ======================= */
  lv_obj_clear_flag(b->line2_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line2_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line2_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(b->line2_label, 2, 0);
  lv_obj_align_to(b->line2_label, b->line1_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0,  0);
  /* =======================
   * 6. line3（Align left）
   * ======================= */
  lv_obj_clear_flag(b->line3_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line3_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line3_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(b->line3_label, 2, 0);
  lv_obj_align_to(b->line3_label, b->line2_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0,  0);
  /* =======================
   * 7. line4（Align left）
   * ======================= */
  lv_obj_clear_flag(b->line4_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line4_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line4_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(b->line4_label, 2, 0);
  lv_obj_align_to(b->line4_label, b->line3_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0,  0);
  /* =======================
   * 8. qmi_line1_label（line5, Align left）
   * ======================= */
  lv_obj_set_width(qmi_line1_label, lv_pct(100));
  lv_obj_set_style_text_align(qmi_line1_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(qmi_line1_label, 2, 0);
  lv_obj_align_to(qmi_line1_label, b->line4_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0,  0);
  /* =======================
   * 9. qmi_line2_label（line6, Align left）
   * ======================= */
  lv_obj_set_width(qmi_line2_label, lv_pct(100));
  lv_obj_set_style_text_align(qmi_line2_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(qmi_line2_label, 2, 0);
  lv_obj_align_to(qmi_line2_label, qmi_line1_label,  LV_ALIGN_OUT_BOTTOM_LEFT, 0,  0);
  /* =======================
   *  10. qmi_line3_label（line7, Align left）
   * ======================= */
  lv_obj_set_width(qmi_line3_label, lv_pct(100));
  lv_obj_set_style_text_align(qmi_line3_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(qmi_line3_label, 2, 0);
  lv_obj_align_to(qmi_line3_label, qmi_line2_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0,  0);
}

static void qmi_ui_apply(const qmi_state_t *st) {
  /* =======================
   * 1. Title & Status Color
   * ======================= */
  example_ui_t *b = ui.base;
  snprintf(b->line1_text, sizeof(b->line1_text), "QMI");
  lv_label_set_text(b->line1_label, b->line1_text);
  lv_color_t title_color;
  switch (st->qmi_init_ret) {
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
   * 2. Format Sensor Data
   * ======================= */
  char ax_str[16], ay_str[16], az_str[16];
  char gx_str[16], gy_str[16], gz_str[16];
  middle_fmt_fixed1(ax_str, sizeof(ax_str),
                               (int32_t)(st->ax * 10.0f));
  middle_fmt_fixed1(ay_str, sizeof(ay_str),
                               (int32_t)(st->ay * 10.0f));
  middle_fmt_fixed1(az_str, sizeof(az_str),
                               (int32_t)(st->az * 10.0f));
  middle_fmt_fixed1(gx_str, sizeof(gx_str),
                               (int32_t)(st->gx * 10.0f));
  middle_fmt_fixed1(gy_str, sizeof(gy_str),
                               (int32_t)(st->gy * 10.0f));
  middle_fmt_fixed1(gz_str, sizeof(gz_str),
                               (int32_t)(st->gz * 10.0f));
  snprintf(b->line2_text, sizeof(b->line2_text), "ax:%s", ax_str);
  snprintf(b->line3_text, sizeof(b->line3_text), "ay:%s", ay_str);
  snprintf(b->line4_text, sizeof(b->line4_text), "az:%s", az_str);
  snprintf(qmi_line1_text, sizeof(qmi_line1_text), "gx:%s", gx_str);
  snprintf(qmi_line2_text, sizeof(qmi_line2_text), "gy:%s", gy_str);
  snprintf(qmi_line3_text, sizeof(qmi_line3_text), "gz:%s", gz_str);
  lv_label_set_text(b->line2_label, b->line2_text);
  lv_label_set_text(b->line3_label, b->line3_text);
  lv_label_set_text(b->line4_label, b->line4_text);
  lv_label_set_text(qmi_line1_label, qmi_line1_text);
  lv_label_set_text(qmi_line2_label, qmi_line2_text);
  lv_label_set_text(qmi_line3_label, qmi_line3_text);
}

static void qmi_data_update(lv_timer_t *t) {
  /* =======================
   * 1. Read Sensor & Refresh UI
   * ======================= */
  float ax = NAN, ay = NAN, az = NAN, gx = NAN, gy = NAN, gz = NAN;
  esp_err_t r = middle_read_qmi(&ax, &ay, &az, &gx, &gy, &gz);
  if (r == ESP_OK) {
    qmi_state.ax = ax;
    qmi_state.ay = ay;
    qmi_state.az = az;
    qmi_state.gx = gx;
    qmi_state.gy = gy;
    qmi_state.gz = gz;
  }
  qmi_ui_apply(&qmi_state);
}

void qmi_start(void) {
  /* =======================
   * 1. Start Display & UI
   * ======================= */
  bool locked = bsp_display_lock(0);
  if (locked) {
    qmi_ui_init();
    bsp_display_unlock();
  }

  /* =======================
   * 2. Start Sensor & Timer
   * ======================= */
  qmi_state.qmi_init_ret = middle_init_qmi8658();
  locked = bsp_display_lock(0);
  if (locked) {
    ui_create_timer(100, qmi_data_update);
    bsp_display_unlock();
  }

  /* =======================
   * 3. Idle Loop
   * ======================= */
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
