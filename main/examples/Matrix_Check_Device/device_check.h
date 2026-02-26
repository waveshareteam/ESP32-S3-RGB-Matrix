#pragma once

#include "esp_err.h"
#include "pcf85063a.h"

#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void device_check_start(void);

bool device_check_only_audio(void);

typedef enum {
    PAGE_SHTC3 = 0,
    PAGE_QMI = 1,
    PAGE_SDCARD = 2,
    PAGE_RTC = 3,
    PAGE_WIFI = 4,
    PAGE_AUDIO = 5,
    PAGE_COUNT
} ui_page_t;

typedef struct {
    esp_err_t qmi_init_ret;
    esp_err_t shtc3_init_ret;
    esp_err_t sd_init_ret;
    esp_err_t rtc_init_ret;
    esp_err_t rtc_read_ret;

    esp_err_t wifi_init_ret;
    bool wifi_sta_configured;
    bool wifi_sta_connected;
    uint32_t wifi_sta_ip;
    int wifi_sta_rssi;
    bool wifi_ap_on;
    int wifi_ap_clients;
    uint32_t wifi_ap_ip;

    float temp_c;
    float hum_rh;

    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;

    size_t sd_total_mb;
    size_t sd_free_mb;
    size_t sd_size_mb;

    pcf85063a_datetime_t rtc_time;

    esp_err_t audio_init_ret;
    esp_err_t audio_open_ret;
    esp_err_t audio_read_ret;
    int audio_rlen;
    int32_t audio_peak_l;
    int32_t audio_peak_r;
    int32_t audio_peak;
    int32_t audio_min;
    int32_t audio_max;

    uint8_t audio_out_vol;
    uint8_t audio_mode;
    bool audio_playing;

    ui_page_t page;
} device_check_state_t;

typedef struct {
    lv_obj_t *page_label;
    lv_obj_t *axis_label;
    lv_obj_t *line1_label;
    lv_obj_t *qmi_line1_r_label;
    lv_obj_t *line2_label;
    lv_obj_t *qmi_line2_r_label;
    lv_obj_t *line3_label;
    lv_obj_t *line4_label;
    lv_obj_t *footer_label;

    char page_text[48];
    char line1_text[96];
    char qmi_line1_r_text[16];
    char line2_text[96];
    char qmi_line2_r_text[16];
    char line3_text[96];
    char line4_text[96];
    char footer_text[16];
} device_check_ui_t;

device_check_state_t *device_check_state(void);
device_check_ui_t *device_check_ui(void);

const char *device_check_err_name(esp_err_t r);
void device_check_ui_apply_page_style(const char *page_name, esp_err_t init_ret);

void device_check_ui_show_rgb_splash(uint32_t total_ms);
void device_check_ui_init_screen(void);
void device_check_ui_update(void);
void device_check_ui_install_timer(uint32_t period_ms);

void device_check_ui_render_shtc3(void);
void device_check_ui_render_qmi(void);
void device_check_ui_render_sdcard(void);
void device_check_ui_render_rtc(void);
void device_check_ui_render_wifi(void);
void device_check_ui_render_audio(void);

