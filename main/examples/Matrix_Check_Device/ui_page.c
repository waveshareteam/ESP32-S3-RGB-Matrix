#include "device_check.h"

#include "rtc_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

void device_check_ui_apply_page_style(const char *page_name, esp_err_t init_ret)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    snprintf(ui->page_text, sizeof(ui->page_text), "%s", page_name);
    lv_label_set_text(ui->page_label, ui->page_text);

    lv_color_t color = lv_color_hex(0xE0E0E0);
    color = (init_ret == ESP_OK) ? lv_color_hex(0x00C000) : color;
    color = (init_ret == ESP_ERR_NOT_SUPPORTED) ? lv_color_hex(0x808080) : color;
    color = (init_ret != ESP_OK && init_ret != ESP_ERR_NOT_SUPPORTED) ? lv_color_hex(0xC00000) : color;
    lv_obj_set_style_text_color(ui->page_label, color, 0);
}
static uint32_t abs_u32(int32_t v)
{
    return (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
}

static void fmt_fixed2(char *out, size_t out_len, int32_t v100)
{
    uint32_t av = abs_u32(v100);
    unsigned ip = (unsigned)(av / 100U);
    unsigned fp = (unsigned)(av % 100U);
    snprintf(out, out_len, "%s%u.%02u", (v100 < 0) ? "-" : "", ip, fp);
}

static void fmt_fixed1(char *out, size_t out_len, int32_t v10)
{
    uint32_t av = abs_u32(v10);
    unsigned ip = (unsigned)(av / 10U);
    unsigned fp = (unsigned)(av % 10U);
    snprintf(out, out_len, "%s%u.%01u", (v10 < 0) ? "-" : "", ip, fp);
}

void device_check_ui_render_shtc3(void)
{
    device_check_state_t *st = device_check_state();
    device_check_ui_t *ui = device_check_ui();
    if (!st || !ui) return;

    device_check_ui_apply_page_style("ST3", st->shtc3_init_ret);

    {
        char temp_str[20];
        char hum_str[20];
        fmt_fixed2(temp_str, sizeof(temp_str), (int32_t)(st->temp_c * 100.0f));
        fmt_fixed2(hum_str, sizeof(hum_str), (int32_t)(st->hum_rh * 100.0f));
        snprintf(ui->line1_text, sizeof(ui->line1_text), "Temp");
        snprintf(ui->line2_text, sizeof(ui->line2_text), "%sC", temp_str);
        snprintf(ui->line3_text, sizeof(ui->line3_text), "Hum");
        snprintf(ui->line4_text, sizeof(ui->line4_text), "%s%%", hum_str);
    }

    lv_label_set_text(ui->line1_label, ui->line1_text);
    lv_label_set_text(ui->line2_label, ui->line2_text);
    lv_label_set_text(ui->line3_label, ui->line3_text);
    lv_label_set_text(ui->line4_label, ui->line4_text);
}

void device_check_ui_render_qmi(void)
{
    device_check_state_t *st = device_check_state();
    device_check_ui_t *ui = device_check_ui();
    if (!st || !ui) return;

    device_check_ui_apply_page_style("QMI", st->qmi_init_ret);

    {
        char ax_str[16], ay_str[16], az_str[16];
        fmt_fixed1(ax_str, sizeof(ax_str), (int32_t)(st->ax * 10.0f));
        fmt_fixed1(ay_str, sizeof(ay_str), (int32_t)(st->ay * 10.0f));
        fmt_fixed1(az_str, sizeof(az_str), (int32_t)(st->az * 10.0f));

        const int32_t gx_dps = (int32_t)st->gx;
        const int32_t gy_dps = (int32_t)st->gy;
        const int32_t gz_dps = (int32_t)st->gz;

        snprintf(ui->line1_text, sizeof(ui->line1_text), "ax:%s", ax_str);
        snprintf(ui->line2_text, sizeof(ui->line2_text), "ay:%s", ay_str);
        snprintf(ui->line3_text, sizeof(ui->line3_text), "az:%s", az_str);
        snprintf(ui->qmi_line1_r_text, sizeof(ui->qmi_line1_r_text), "gx:%ld", (long)gx_dps);
        snprintf(ui->qmi_line2_r_text, sizeof(ui->qmi_line2_r_text), "gy:%ld", (long)gy_dps);
        snprintf(ui->line4_text, sizeof(ui->line4_text), "gz:%ld", (long)gz_dps);
    }

    lv_label_set_text(ui->line1_label, ui->line1_text);
    lv_label_set_text(ui->qmi_line1_r_label, ui->qmi_line1_r_text);
    lv_label_set_text(ui->line2_label, ui->line2_text);
    lv_label_set_text(ui->qmi_line2_r_label, ui->qmi_line2_r_text);
    lv_label_set_text(ui->line3_label, ui->line3_text);
    lv_label_set_text(ui->line4_label, ui->line4_text);
}

void device_check_ui_render_sdcard(void)
{
    device_check_state_t *st = device_check_state();
    device_check_ui_t *ui = device_check_ui();
    if (!st || !ui) return;

    device_check_ui_apply_page_style("SD", st->sd_init_ret);
    snprintf(ui->line1_text, sizeof(ui->line1_text), "Mem");

    {
        snprintf(ui->line2_text, sizeof(ui->line2_text), "---");
        const bool ok = (st->sd_init_ret == ESP_OK) && (st->sd_size_mb > 0);
        if (ok) {
            uint32_t mb = (uint32_t)st->sd_size_mb;
            uint32_t gb10 = (mb * 10U + 512U) / 1024U;
            unsigned gb_i = (unsigned)(gb10 / 10U);
            unsigned gb_f = (unsigned)(gb10 % 10U);
            snprintf(ui->line2_text, sizeof(ui->line2_text), "%u.%uG", gb_i, gb_f);
        }
    }

    ui->line3_text[0] = '\0';
    ui->line4_text[0] = '\0';
    lv_label_set_text(ui->line1_label, ui->line1_text);
    lv_label_set_text(ui->line2_label, ui->line2_text);
    lv_label_set_text(ui->line3_label, ui->line3_text);
    lv_label_set_text(ui->line4_label, ui->line4_text);
}
void device_check_ui_render_rtc(void)
{
    device_check_state_t *st = device_check_state();
    device_check_ui_t *ui = device_check_ui();
    if (!st || !ui) return;

    device_check_ui_apply_page_style("RTC", st->rtc_init_ret);

    {
        const esp_err_t init_r = st->rtc_init_ret;
        const esp_err_t rr = st->rtc_read_ret;
        const bool ok = (init_r == ESP_OK) && (rr == ESP_OK);
        const pcf85063a_datetime_t t = st->rtc_time;

        if (ok) {
            snprintf(ui->line1_text, sizeof(ui->line1_text), "%04u", (unsigned)t.year);
            snprintf(ui->line2_text, sizeof(ui->line2_text), "%02u-%02u", (unsigned)t.month, (unsigned)t.day);
            snprintf(ui->line3_text, sizeof(ui->line3_text), "%02u:%02u:%02u", (unsigned)t.hour, (unsigned)t.min,
                     (unsigned)t.sec);
            snprintf(ui->line4_text, sizeof(ui->line4_text), "alarm:%u", (unsigned)(rtc_service_alarm_seq() & 1U));
        }
        if (!ok && init_r != ESP_OK) {
            snprintf(ui->line1_text, sizeof(ui->line1_text), "init");
            snprintf(ui->line2_text, sizeof(ui->line2_text), "%s", device_check_err_name(init_r));
            snprintf(ui->line3_text, sizeof(ui->line3_text), "----");
            snprintf(ui->line4_text, sizeof(ui->line4_text), "----");
        }
        if (!ok && init_r == ESP_OK) {
            snprintf(ui->line1_text, sizeof(ui->line1_text), "read");
            snprintf(ui->line2_text, sizeof(ui->line2_text), "%s", device_check_err_name(rr));
            snprintf(ui->line3_text, sizeof(ui->line3_text), "----");
            snprintf(ui->line4_text, sizeof(ui->line4_text), "----");
        }
    }

    lv_label_set_text(ui->line1_label, ui->line1_text);
    lv_label_set_text(ui->line2_label, ui->line2_text);
    lv_label_set_text(ui->line3_label, ui->line3_text);
    lv_label_set_text(ui->line4_label, ui->line4_text);
}

void device_check_ui_render_wifi(void)
{
    device_check_state_t *st = device_check_state();
    device_check_ui_t *ui = device_check_ui();
    if (!st || !ui) return;

    device_check_ui_apply_page_style("WIFI", st->wifi_init_ret);

    {
        const esp_err_t ir = st->wifi_init_ret;
        const bool ok = (ir == ESP_OK);
        const unsigned sta1 = ok ? (unsigned)(st->wifi_sta_connected != 0) : 0U;
        const int rssi = (ok && sta1) ? st->wifi_sta_rssi : 0;

        if (ok) {
            snprintf(ui->line1_text, sizeof(ui->line1_text), "AP%u", (unsigned)st->wifi_ap_clients);
            snprintf(ui->line2_text, sizeof(ui->line2_text), "STA%u", sta1);
            snprintf(ui->line3_text, sizeof(ui->line3_text), "RSSI:%d", rssi);
            ui->line4_text[0] = '\0';
        }
        if (!ok) {
            snprintf(ui->line1_text, sizeof(ui->line1_text), "init");
            snprintf(ui->line2_text, sizeof(ui->line2_text), "%s", device_check_err_name(ir));
            ui->line3_text[0] = '\0';
            ui->line4_text[0] = '\0';
        }
    }

    lv_label_set_text(ui->line1_label, ui->line1_text);
    lv_label_set_text(ui->line2_label, ui->line2_text);
    lv_label_set_text(ui->line3_label, ui->line3_text);
    lv_label_set_text(ui->line4_label, ui->line4_text);
}

void device_check_ui_render_audio(void)
{
    device_check_state_t *st = device_check_state();
    device_check_ui_t *ui = device_check_ui();
    if (!st || !ui) return;

    const esp_err_t style_ret = (st->audio_init_ret == ESP_OK) ? st->audio_open_ret : st->audio_init_ret;
    device_check_ui_apply_page_style("AUD", style_ret);

    {
        const esp_err_t ir = st->audio_init_ret;
        const esp_err_t orr = st->audio_open_ret;
        const bool ok = (ir == ESP_OK) && (orr == ESP_OK);

        const bool mic_ok = ok && (st->audio_read_ret == ESP_OK) && (st->audio_rlen > 0);
        snprintf(ui->line1_text, sizeof(ui->line1_text), "M1:%s", mic_ok ? "" : "---");
        snprintf(ui->line2_text, sizeof(ui->line2_text), "M2:%s", mic_ok ? "" : "---");
        if (mic_ok) {
            snprintf(ui->line1_text, sizeof(ui->line1_text), "M1:%ld", (long)st->audio_peak_l);
            snprintf(ui->line2_text, sizeof(ui->line2_text), "M2:%ld", (long)st->audio_peak_r);
        }

        snprintf(ui->line3_text, sizeof(ui->line3_text), "VOL %u", (unsigned)st->audio_out_vol);
        snprintf(ui->line4_text, sizeof(ui->line4_text), "%s",
                 ok ? ((st->audio_mode == 1) ? "REC" : (st->audio_playing ? "PLAY" : "STOP")) : "");
    }

    lv_label_set_text(ui->line1_label, ui->line1_text);
    lv_label_set_text(ui->line2_label, ui->line2_text);
    lv_label_set_text(ui->line3_label, ui->line3_text);
    lv_label_set_text(ui->line4_label, ui->line4_text);
}

static void ui_layout_default(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    lv_obj_add_flag(ui->axis_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line1_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line2_r_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(ui->line1_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line2_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line3_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line4_label, LV_FONT_DEFAULT, 0);

    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line1_label, ui->page_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_clear_flag(ui->line2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line2_label, ui->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);

    lv_obj_clear_flag(ui->line3_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line3_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line3_label, ui->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);

    lv_obj_add_flag(ui->line4_label, LV_OBJ_FLAG_HIDDEN);
}

static void ui_layout_shtc3_centered(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    lv_obj_add_flag(ui->axis_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line1_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line2_r_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(ui->line4_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(ui->line1_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line2_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line3_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line4_label, LV_FONT_DEFAULT, 0);

    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line1_label, ui->page_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_clear_flag(ui->line2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line2_label, ui->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_clear_flag(ui->line3_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line3_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line3_label, ui->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    lv_obj_set_width(ui->line4_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line4_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line4_label, ui->line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
}

static void ui_layout_sd_centered(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    lv_obj_add_flag(ui->axis_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line1_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line2_r_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(ui->line1_label, &lv_font_unscii_16, 0);
    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line1_label, ui->page_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);

    lv_obj_clear_flag(ui->line2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(ui->line2_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line2_label, ui->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);

    lv_obj_add_flag(ui->line3_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->line4_label, LV_OBJ_FLAG_HIDDEN);
}

static void ui_layout_aud_centered(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    lv_obj_add_flag(ui->axis_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line1_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line2_r_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(ui->line1_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line1_label, ui->page_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);

    lv_obj_clear_flag(ui->line2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(ui->line2_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line2_label, ui->line1_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    lv_obj_clear_flag(ui->line3_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(ui->line3_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line3_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line3_label, ui->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);

    lv_obj_clear_flag(ui->line4_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(ui->line4_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line4_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line4_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line4_label, ui->line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);
}

static void ui_layout_aud_2line(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    lv_obj_add_flag(ui->axis_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line1_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->qmi_line2_r_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(ui->line1_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->line1_label, 2, 0);
    lv_obj_align_to(ui->line1_label, ui->page_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);

    lv_obj_clear_flag(ui->line2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(ui->line2_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->line2_label, 2, 0);
    lv_obj_align_to(ui->line2_label, ui->line1_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    lv_obj_clear_flag(ui->line3_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(ui->line3_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line3_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line3_label, ui->line2_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);

    lv_obj_clear_flag(ui->line4_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(ui->line4_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(ui->line4_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line4_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui->line4_label, ui->line3_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);
}

static void ui_layout_qmi_6x1_ag(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    lv_obj_add_flag(ui->axis_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_text_font(ui->line1_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->qmi_line1_r_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line2_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->qmi_line2_r_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line3_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_font(ui->line4_label, LV_FONT_DEFAULT, 0);

    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->line1_label, 2, 0);
    lv_obj_align_to(ui->line1_label, ui->page_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_clear_flag(ui->line2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->line2_label, 2, 0);
    lv_obj_align_to(ui->line2_label, ui->line1_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_clear_flag(ui->line3_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line3_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->line3_label, 2, 0);
    lv_obj_align_to(ui->line3_label, ui->line2_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_clear_flag(ui->qmi_line1_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->qmi_line1_r_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->qmi_line1_r_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->qmi_line1_r_label, 2, 0);
    lv_obj_align_to(ui->qmi_line1_r_label, ui->line3_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_clear_flag(ui->qmi_line2_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->qmi_line2_r_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->qmi_line2_r_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->qmi_line2_r_label, 2, 0);
    lv_obj_align_to(ui->qmi_line2_r_label, ui->qmi_line1_r_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_clear_flag(ui->line4_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(ui->line4_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line4_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->line4_label, 2, 0);
    lv_obj_align_to(ui->line4_label, ui->qmi_line2_r_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
}

typedef void (*ui_layout_fn_t)(void);
typedef void (*ui_render_fn_t)(void);

typedef struct {
    ui_layout_fn_t layout;
    ui_render_fn_t render;
} ui_page_desc_t;

static void ui_render_unknown(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    device_check_ui_apply_page_style("?", ESP_FAIL);
    ui->line1_text[0] = '\0';
    ui->line2_text[0] = '\0';
    lv_label_set_text(ui->line1_label, ui->line1_text);
    lv_label_set_text(ui->line2_label, ui->line2_text);
}

static const ui_page_desc_t *ui_get_page_desc(ui_page_t page)
{
    static const ui_page_desc_t s_pages[PAGE_COUNT] = {
        [PAGE_SHTC3] = { ui_layout_shtc3_centered, device_check_ui_render_shtc3 },
        [PAGE_QMI] = { ui_layout_qmi_6x1_ag, device_check_ui_render_qmi },
        [PAGE_SDCARD] = { ui_layout_sd_centered, device_check_ui_render_sdcard },
        [PAGE_RTC] = { ui_layout_shtc3_centered, device_check_ui_render_rtc },
        [PAGE_WIFI] = { ui_layout_aud_centered, device_check_ui_render_wifi },
        [PAGE_AUDIO] = { ui_layout_aud_2line, device_check_ui_render_audio },
    };

    return (page < PAGE_COUNT) ? &s_pages[page] : NULL;
}

void device_check_ui_update(void)
{
    device_check_state_t *st = device_check_state();
    device_check_ui_t *ui = device_check_ui();
    if (!st || !ui) return;

    static ui_page_t s_layout_page = PAGE_COUNT;

    const ui_page_desc_t fallback = { ui_layout_default, ui_render_unknown };
    const ui_page_t page = st->page;
    const ui_page_desc_t *desc = ui_get_page_desc(page);
    const ui_page_desc_t *use = desc ? desc : &fallback;

    if (s_layout_page != page) {
        s_layout_page = page;
        if (use->layout) use->layout();
    }

    const int page_total = device_check_only_audio() ? 1 : (int)PAGE_COUNT;
    const int page_cur = device_check_only_audio() ? 1 : ((int)page + 1);
    snprintf(ui->footer_text, sizeof(ui->footer_text), "%d/%d", page_cur, page_total);
    lv_label_set_text(ui->footer_label, ui->footer_text);

    if (use->render) use->render();
}

static void ui_timer_cb(lv_timer_t *t)
{
    (void)t;
    device_check_ui_update();
}

void device_check_ui_install_timer(uint32_t period_ms)
{
    static lv_timer_t *s_timer = NULL;
    if (s_timer) return;
    s_timer = lv_timer_create(ui_timer_cb, period_ms, NULL);
}

void device_check_ui_show_rgb_splash(uint32_t total_ms)
{
    lv_obj_t *scr = lv_screen_active();
    const uint32_t step_ms = total_ms / 3;
    const uint32_t last_ms = total_ms - step_ms - step_ms;

    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_refr_now(NULL);
    vTaskDelay(pdMS_TO_TICKS(step_ms));

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x00FF00), 0);
    lv_refr_now(NULL);
    vTaskDelay(pdMS_TO_TICKS(step_ms));

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0000FF), 0);
    lv_refr_now(NULL);
    vTaskDelay(pdMS_TO_TICKS(last_ms));
}

void device_check_ui_init_screen(void)
{
    device_check_ui_t *ui = device_check_ui();
    if (!ui) return;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_opa(scr, LV_OPA_COVER, 0);

    ui->page_label = lv_label_create(scr);
    lv_label_set_text(ui->page_label, "");
    lv_label_set_long_mode(ui->page_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui->page_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->page_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(ui->page_label, 2, 0);
    lv_obj_set_style_pad_right(ui->page_label, 18, 0);
    lv_obj_align(ui->page_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_opa(ui->page_label, LV_OPA_COVER, 0);

    ui->axis_label = lv_label_create(scr);
    lv_label_set_text(ui->axis_label, "axyz gxyz");
    lv_obj_set_style_text_color(ui->axis_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_opa(ui->axis_label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(ui->axis_label, LV_FONT_DEFAULT, 0);
    lv_obj_add_flag(ui->axis_label, LV_OBJ_FLAG_HIDDEN);

    ui->line1_label = lv_label_create(scr);
    lv_label_set_text(ui->line1_label, "");
    lv_label_set_long_mode(ui->line1_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui->line1_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line1_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(ui->line1_label, LV_OPA_COVER, 0);

    ui->qmi_line1_r_label = lv_label_create(scr);
    lv_label_set_text(ui->qmi_line1_r_label, "");
    lv_label_set_long_mode(ui->qmi_line1_r_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui->qmi_line1_r_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->qmi_line1_r_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(ui->qmi_line1_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_opa(ui->qmi_line1_r_label, LV_OPA_COVER, 0);

    ui->line2_label = lv_label_create(scr);
    lv_label_set_text(ui->line2_label, "");
    lv_label_set_long_mode(ui->line2_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui->line2_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line2_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(ui->line2_label, LV_OPA_COVER, 0);

    ui->qmi_line2_r_label = lv_label_create(scr);
    lv_label_set_text(ui->qmi_line2_r_label, "");
    lv_label_set_long_mode(ui->qmi_line2_r_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui->qmi_line2_r_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->qmi_line2_r_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(ui->qmi_line2_r_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_opa(ui->qmi_line2_r_label, LV_OPA_COVER, 0);

    ui->line3_label = lv_label_create(scr);
    lv_label_set_text(ui->line3_label, "");
    lv_label_set_long_mode(ui->line3_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line3_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(ui->line3_label, LV_OPA_COVER, 0);

    ui->line4_label = lv_label_create(scr);
    lv_label_set_text(ui->line4_label, "");
    lv_label_set_long_mode(ui->line4_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ui->line4_label, lv_pct(100));
    lv_obj_set_style_text_align(ui->line4_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(ui->line4_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_opa(ui->line4_label, LV_OPA_COVER, 0);

    ui->footer_label = lv_label_create(scr);
    lv_label_set_text(ui->footer_label, "");
    lv_obj_set_style_text_color(ui->footer_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_opa(ui->footer_label, LV_OPA_COVER, 0);
    lv_obj_align(ui->footer_label, LV_ALIGN_TOP_RIGHT, -2, 0);
}
