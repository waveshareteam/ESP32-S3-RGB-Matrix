#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common_ui.h"
#include "middle_wifi.h"
#include <stdio.h>

#define WIFI_STA_SSID "CQ793"
#define WIFI_STA_PASS "123456789"

typedef struct {
  esp_err_t wifi_init_ret;
  bool wifi_sta_connected;
  int wifi_sta_rssi;
  int wifi_ap_clients;
} wifi_state_t;

typedef struct {
  common_ui *base;
} WiFi_UI;

static WiFi_UI ui;
static wifi_state_t wifi_state;

static void wifi_ui_init(void) {
  /* =======================
   * 1. Create Base UI
   * ======================= */
  ui.base = common_ui_get();
  common_ui_init();
  common_ui *b = ui.base;
  /* =======================
   * 2. Layout & Fonts
   * ======================= */
  lv_obj_set_style_text_font(b->line1_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(b->line2_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(b->line3_label, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_font(b->line4_label, LV_FONT_DEFAULT, 0);
  /* =======================
   * 3. line1 (Align Top Left)
   * ======================= */
  lv_obj_set_width(b->line1_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line1_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(b->line1_label, LV_ALIGN_TOP_MID, 0, -1);
  /* =======================
   * 4. line2 (Align Mid)
   * ======================= */
  lv_obj_clear_flag(b->line2_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line2_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line2_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line2_label, b->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0,-1);
  /* =======================
   * 5. line3 (Align Mid)
   * ======================= */
  lv_obj_clear_flag(b->line3_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line3_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line3_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line3_label, b->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0,-1);
  /* =======================
   * 6. line4 (Align Mid)
   * ======================= */
  lv_obj_clear_flag(b->line4_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(b->line4_label, lv_pct(100));
  lv_obj_set_style_text_align(b->line4_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(b->line4_label, b->line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0,-1);
}

static void wifi_ui_apply(const wifi_state_t *st) {
  /* =======================
   * 1. Title & Status Color
   * ======================= */
  example_ui_t *b = ui.base;
  snprintf(b->line1_text, sizeof(b->line1_text), "WIFI");
  lv_label_set_text(b->line1_label, b->line1_text);
  switch (st->wifi_init_ret) {
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
   * 2. Format Status Text
   * ======================= */
  if (st->wifi_init_ret == ESP_OK) {
    snprintf(b->line2_text, sizeof(b->line2_text), "AP:%u",
             (unsigned)st->wifi_ap_clients);
    snprintf(b->line3_text, sizeof(b->line3_text), "STA:%u",
             (unsigned)(st->wifi_sta_connected != 0));
    snprintf(b->line4_text, sizeof(b->line4_text), "RSSI:%d",
             st->wifi_sta_rssi);
  } else {
    snprintf(b->line2_text, sizeof(b->line2_text), "Error");
    snprintf(b->line3_text, sizeof(b->line3_text), "R:%d", st->wifi_init_ret);
    b->line4_text[0] = '\0';
  }
  lv_label_set_text(b->line2_label, b->line2_text);
  lv_label_set_text(b->line3_label, b->line3_text);
  lv_label_set_text(b->line4_label, b->line4_text);
}

static void wifi_data_update(lv_timer_t *t) {
  /* =======================
   * 1. Query WiFi Status & Refresh UI
   * ======================= */
  middle_wifi_status_t status;
  esp_err_t r = middle_wifi_get_status(&status);
  wifi_state.wifi_init_ret = r;
  if (r == ESP_OK) {
    wifi_state.wifi_sta_connected = status.sta_connected;
    wifi_state.wifi_sta_rssi = status.sta_rssi;
    wifi_state.wifi_ap_clients = status.ap_clients;
  }
  wifi_ui_apply(&wifi_state);
}

void wifi_start(void) {
  /* =======================
   * 1. Start Display & UI
   * ======================= */
  bool locked = bsp_display_lock(0);
  if (locked) {
    wifi_ui_init();
    bsp_display_unlock();
  }

  /* =======================
   * 2. Configure & Enable WiFi
   * ======================= */
  middle_wifi_set_sta_config(WIFI_STA_SSID, WIFI_STA_PASS);
  wifi_state.wifi_init_ret = middle_wifi_init();

  /* =======================
   * 3. Periodic Refresh
   * ======================= */
  locked = bsp_display_lock(0);
  if (locked) {
    ui_create_timer(1000, wifi_data_update);
    bsp_display_unlock();
  }

  /* =======================
   * 4. Idle Loop
   * ======================= */
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
