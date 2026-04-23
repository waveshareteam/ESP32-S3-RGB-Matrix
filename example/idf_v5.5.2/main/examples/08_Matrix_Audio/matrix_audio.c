
#include "common_ui.h"
#include "middle_audio.h"
#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_CHANNELS    2
#define AUDIO_BITS        16
#define VOLUME            80

typedef struct {
    int audio_rlen;
    esp_err_t audio_init_ret;
    esp_err_t audio_open_ret;
    esp_err_t audio_read_ret;
    int32_t audio_peak_l;
    int32_t audio_peak_r;
    uint8_t audio_out_vol;
    uint8_t audio_mode;
    } audio_state_t;

typedef struct {
    example_ui_t *base;
} Audio_UI;

static int32_t abs_i32(int32_t v) { return v < 0 ? -v : v; }

static audio_state_t audio_state;
static Audio_UI ui;
static lv_obj_t *m1_val_label = NULL;
static lv_obj_t *m2_val_label = NULL;
static lv_obj_t *status_label = NULL;
static char status_text[32];

static void audio_ui_init(void) {
    /* =======================
     * 1. Init Common UI
     * ======================= */
    ui.base = common_ui_get();
    
    common_ui_init();
    example_ui_t *b = ui.base;
    lv_obj_t *scr = lv_scr_act();

    /* =======================
     * 2. Create Extra Labels
     * ======================= */
    m1_val_label = lv_label_create(scr);
    m2_val_label = lv_label_create(scr);
    status_label = lv_label_create(scr);
    lv_obj_set_style_text_color(m1_val_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(m2_val_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);

    /* =======================
     * 3. line1 (Align Top Left)
     * ======================= */
    lv_obj_set_style_text_font(b->line1_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(b->line1_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(b->line1_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(b->line1_label, 2, 0);
    lv_obj_align(b->line1_label, LV_ALIGN_TOP_LEFT, 0, 2);

    /* =======================
     * 4. line2 (Align Top Left)
     * ======================= */
    lv_obj_clear_flag(b->line2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(b->line2_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(b->line2_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(b->line2_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(b->line2_label, 2, 0);
    lv_obj_align_to(b->line2_label, b->line1_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    /* =======================
     * 5. m1_val_label (Align Left)
     * ======================= */
    lv_obj_set_style_text_font(m1_val_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(m1_val_label, lv_pct(100));
    lv_obj_set_style_text_align(m1_val_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(m1_val_label, 2, 0);
    lv_obj_align_to(m1_val_label, b->line2_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);

    /* =======================
     * 6. line3 (Align Top Left)
     * ======================= */
    lv_obj_clear_flag(b->line3_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(b->line3_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(b->line3_label, lv_pct(100));
    lv_obj_set_style_text_align(b->line3_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(b->line3_label, 2, 0);
    lv_obj_align_to(b->line3_label, m1_val_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    /* =======================
     * 7. m2_val_label (Align Left)
     * ======================= */
    lv_obj_set_style_text_font(m2_val_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(m2_val_label, lv_pct(100));
    lv_obj_set_style_text_align(m2_val_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(m2_val_label, 2, 0);
    lv_obj_align_to(m2_val_label, b->line3_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 1);

    /* =======================
     * 8. line4 (Align Top Left)
     * ======================= */
    lv_obj_clear_flag(b->line4_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(b->line4_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(b->line4_label, lv_pct(100));
    lv_obj_set_style_text_align(b->line4_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(b->line4_label, 2, 0);
    lv_obj_align_to(b->line4_label, m2_val_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    /* =======================
     * 9. status_label (Align Top Right)
     * ======================= */
    lv_obj_set_style_text_font(status_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_width(status_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -2, 2);
}

static void audio_ui_apply(const audio_state_t *st) {
    /* =======================
     * 1. Title & Status Color
     * ======================= */
    example_ui_t *b = ui.base;
    const esp_err_t style_ret = (st->audio_init_ret != ESP_OK) ? st->audio_init_ret : st->audio_open_ret;
    snprintf(b->line1_text, sizeof(b->line1_text), "ECHO");
    lv_label_set_text(b->line1_label, b->line1_text);

    switch (style_ret) {
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
     * 2. Basic Text
     * ======================= */
    const bool ok = (st->audio_init_ret == ESP_OK) && (st->audio_open_ret == ESP_OK) && (st->audio_read_ret == ESP_OK);
    snprintf(status_text, sizeof(status_text), "%s", ok ? "OK" : "ERR");
    snprintf(b->line2_text, sizeof(b->line2_text), "M1:");
    snprintf(b->line3_text, sizeof(b->line3_text), "M2:");
    snprintf(b->line4_text, sizeof(b->line4_text), "VOL:%u", (unsigned)st->audio_out_vol);
    lv_label_set_text(b->line2_label, b->line2_text);
    lv_label_set_text(b->line3_label, b->line3_text);
    lv_label_set_text(b->line4_label, b->line4_text);
    lv_label_set_text(status_label, status_text);

    if (!m1_val_label || !m2_val_label) return;

    if (!ok) {
        lv_label_set_text(m1_val_label, "---");
        lv_label_set_text(m2_val_label, "---");
        return;
    }

    snprintf(b->line1_text, sizeof(b->line1_text), "%ld", (long)st->audio_peak_l);
    snprintf(b->line2_text, sizeof(b->line2_text), "%ld", (long)st->audio_peak_r);
    lv_label_set_text(m1_val_label, b->line1_text);
    lv_label_set_text(m2_val_label, b->line2_text);
}

static void audio_data_update(lv_timer_t *t) {
    audio_ui_apply(&audio_state);
}

static void audio_task(void *arg) {
    (void)arg;
    /* =======================
     * 1. Init Audio Pipeline
     * ======================= */
    static int16_t in_buf[512];
    audio_state.audio_init_ret = middle_audio_init(true, true);
    audio_state.audio_open_ret = audio_state.audio_init_ret;
    middle_audio_open(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_BITS);
    middle_audio_set_out_vol(audio_state.audio_out_vol);
    middle_audio_set_in_gain(30.0f);
    middle_audio_set_out_mute(false);

    /* =======================
     * 2. Read Mic -> Playback & Measure Peak
     * ======================= */
    while (true) {
        if (audio_state.audio_open_ret != ESP_OK) {
            audio_state.audio_read_ret = audio_state.audio_open_ret;
            audio_state.audio_rlen = 0;
            audio_state.audio_peak_l = 0;
            audio_state.audio_peak_r = 0;
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        size_t br = 0;
        esp_err_t r = middle_audio_read_i2s(in_buf, sizeof(in_buf), &br, 20);
        audio_state.audio_read_ret = r;
        audio_state.audio_rlen = (int)br;

        if (r == ESP_OK && br > 0) {
            middle_audio_write(in_buf, br);

            int32_t peak_l = 0, peak_r = 0;
            const int in_n = (int)br / (int)sizeof(int16_t);
            for (int i = 0; i + 1 < in_n; i += 2) {
                int32_t al = abs_i32((int32_t)in_buf[i]);
                int32_t ar = abs_i32((int32_t)in_buf[i + 1]);
                peak_l = al > peak_l ? al : peak_l;
                peak_r = ar > peak_r ? ar : peak_r;
            }
            audio_state.audio_peak_l = peak_l;
            audio_state.audio_peak_r = peak_r;
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void audio_start(void) {
    /* =======================
     * 1. Start Display & UI
     * ======================= */
    bool locked = bsp_display_lock(0);
    if (locked) {
        audio_ui_init();
        bsp_display_unlock();
    }
    /* =======================
     * 2. Start Audio Task
     * ======================= */
    audio_state.audio_out_vol = VOLUME;
    xTaskCreate(audio_task, "aud", 4096, NULL, tskIDLE_PRIORITY + 4, NULL);
    /* =======================
     * 3. Periodic UI Refresh
     * ======================= */
    locked = bsp_display_lock(0);
    if (locked) {
        ui_create_timer(100, audio_data_update);
        bsp_display_unlock();
    }
    /* =======================
     * 4. Idle Loop
     * ======================= */
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
