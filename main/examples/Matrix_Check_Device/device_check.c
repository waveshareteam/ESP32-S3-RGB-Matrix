#include "device_check.h"
#include "lv_conf_internal.h"
#include "lvgl.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "extra_bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdcard_service.h"
#include "wifi_service.h"
#include "sensor_service.h"
#include "audio_service.h"
#include "key_service.h"
#include "rtc_service.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "device_check";

static const bool s_only_aud = false;

bool device_check_only_audio(void)
{
    return s_only_aud;
}

static void gen_square(int16_t *out, int n, int16_t amp, int period)
{
    if (!out || n <= 0 || period <= 0) return;

    static int s_phase = 0;
    const int half = period >> 1;
    int i = 0;
    while (i < n) {
        const int p = s_phase++;
        const int m = p - (p / period) * period;
        out[i] = (m < half) ? amp : (int16_t)-amp;
        i++;
    }
}

static device_check_state_t s_state;

//RTC闹钟标志位
static volatile uint8_t s_rtc_arm_req = 0;

static device_check_ui_t s_ui;

device_check_state_t *device_check_state(void)
{
    return &s_state;
}

device_check_ui_t *device_check_ui(void)
{
    return &s_ui;
}

static TaskHandle_t s_lvgl_task = NULL;

static volatile uint8_t s_aud_out_vol = 100;
static volatile TickType_t s_aud_play_deadline = 0;
static volatile uint8_t s_aud_mode = 0;
static volatile uint8_t s_aud_rec_req = 0;

static int16_t *s_aud_rec_mono = NULL;

//********************************************   IO0处理任务   ********************************************//
static void device_check_next_page(void)
{
    if (s_only_aud) {
        s_state.page = PAGE_AUDIO;
        return;
    }

    const ui_page_t next = (ui_page_t)(((int)s_state.page + 1) % (int)PAGE_COUNT);
    s_state.page = next;
    if (next == PAGE_RTC) s_rtc_arm_req = 1;
}

static void device_check_key_cb(key_service_evt_t evt, void *user)
{
    (void)user;

    switch (evt) {
    case KEY_SERVICE_EVT_CLICK:
        device_check_next_page();
        return;
    case KEY_SERVICE_EVT_DOUBLE_CLICK:
        if (s_state.page != PAGE_AUDIO) {
            device_check_next_page();
            return;
        }
        s_aud_rec_req = 1;
        s_aud_play_deadline = 0;
        return;
    case KEY_SERVICE_EVT_LONG_REPEAT: {
        uint8_t v = s_aud_out_vol;
        const uint16_t t = (uint16_t)v + 20U;
        v = (uint8_t)((t > 100U) ? 0U : t);
        s_aud_out_vol = v;
        s_state.audio_out_vol = v;
        audio_service_set_out_vol(v);
        return;
    }
    default:
        return;
    }
}

//********************************************   audio处理任务   ********************************************//
static int32_t abs_i32(int32_t v);//计算绝对值
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi);//限制值在范围内

static void audio_task(void *arg)
{
    static int16_t in_buf[1200];
    static int16_t out_buf[1200];
    uint32_t rec_pos = 0;
    uint32_t play_pos = 0;
    const uint32_t rec_total = 16000U * 3U;
    TickType_t rec_start_tick = 0;
    TickType_t rec_end_tick = 0;

    s_state.audio_init_ret = audio_service_init();
    s_state.audio_open_ret = s_state.audio_init_ret;
    if (s_state.audio_init_ret == ESP_OK) s_state.audio_open_ret = audio_service_open(16000, 2, 16);
    if (s_state.audio_open_ret == ESP_OK) audio_service_set_out_vol(s_aud_out_vol);
    if (s_state.audio_open_ret == ESP_OK) audio_service_set_in_gain(30.0f);
    if (s_state.audio_open_ret == ESP_OK) audio_service_set_out_mute(true);

    while (true) {
        if (s_state.audio_open_ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (s_aud_rec_req != 0) {
            s_aud_rec_req = 0;
            if (!s_aud_rec_mono) {
                s_aud_rec_mono = (int16_t *)malloc((size_t)rec_total * sizeof(int16_t));
                if (!s_aud_rec_mono) {
                    wifi_service_set_enabled(false);
                    s_aud_rec_mono = (int16_t *)malloc((size_t)rec_total * sizeof(int16_t));
                }
                if (!s_aud_rec_mono) {
                    s_aud_mode = 0;
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
            }
            s_aud_mode = 1;
            rec_pos = 0;
            play_pos = 0;
            s_aud_play_deadline = 0;
            rec_start_tick = xTaskGetTickCount();
            rec_end_tick = rec_start_tick + pdMS_TO_TICKS(3000);
        }

        const TickType_t now_tick = xTaskGetTickCount();
        const TickType_t end = s_aud_play_deadline;
        const bool playing = (s_aud_mode == 2) && (end != 0) && ((int32_t)(end - now_tick) > 0);
        s_state.audio_out_vol = s_aud_out_vol;
        s_state.audio_mode = s_aud_mode;
        s_state.audio_playing = playing;
        audio_service_set_out_vol(s_aud_out_vol);
        audio_service_set_out_mute(!playing);

        if (s_aud_mode == 2 && !playing) {
            s_aud_mode = 0;
            s_aud_play_deadline = 0;
            play_pos = 0;
            free(s_aud_rec_mono);
            s_aud_rec_mono = NULL;
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (s_aud_mode == 2) {
            const int max_frames = (int)(sizeof(out_buf) / (sizeof(int16_t) * 2));
            const uint32_t remain = (play_pos < rec_total) ? (rec_total - play_pos) : 0;
            const int frames = (remain < (uint32_t)max_frames) ? (int)remain : max_frames;

            if (frames <= 0) {
                s_aud_mode = 0;
                s_aud_play_deadline = 0;
                free(s_aud_rec_mono);
                s_aud_rec_mono = NULL;
                vTaskDelay(pdMS_TO_TICKS(2));
                continue;
            }

            int i = 0;
            while (i < frames) {
                const int16_t s = s_aud_rec_mono ? s_aud_rec_mono[play_pos + (uint32_t)i] : 0;
                out_buf[i * 2] = s;
                out_buf[i * 2 + 1] = s;
                i++;
            }

            play_pos += (uint32_t)frames;
            const int wlen = frames * (int)(sizeof(int16_t) * 2);

            s_state.audio_rlen = 0;
            s_state.audio_read_ret = ESP_OK;

            int32_t minv = 32767;
            int32_t maxv = -32768;
            int32_t peak = 0;
            int j = 0;
            while (j < frames) {
                const int32_t v = (int32_t)out_buf[j * 2];
                minv = v < minv ? v : minv;
                maxv = v > maxv ? v : maxv;
                const int32_t a = abs_i32(v);
                peak = a > peak ? a : peak;
                j++;
            }
            s_state.audio_min = minv;
            s_state.audio_max = maxv;
            s_state.audio_peak = peak;
            s_state.audio_peak_l = peak;
            s_state.audio_peak_r = peak;

            if (playing && wlen > 0) audio_service_write((void *)out_buf, (size_t)wlen);

            if (play_pos >= rec_total) {
                s_aud_mode = 0;
                s_aud_play_deadline = 0;
                free(s_aud_rec_mono);
                s_aud_rec_mono = NULL;
            }

            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        const uint8_t aud_mode = s_aud_mode;
        const uint32_t to_ms = (aud_mode == 1) ? 200U : 20U;
        size_t br = 0;
        const esp_err_t rr = audio_service_read_i2s(in_buf, sizeof(in_buf), &br, (uint32_t)to_ms);
        const bool mic_ok = (rr == ESP_OK) && (br > 0);

        const int bytes = (int)sizeof(out_buf);
        const int n = bytes / (int)sizeof(int16_t);
        if (!mic_ok && aud_mode != 1) gen_square(out_buf, n, 2500, 40);

        int wlen = mic_ok ? (int)br : bytes;
        const int16_t *wbuf = mic_ok ? in_buf : out_buf;

        s_state.audio_rlen = (int)br;
        s_state.audio_read_ret = rr;

        int32_t in_peak_l = 0;
        int32_t in_peak_r = 0;
        if (mic_ok) {
            const int in_n = (int)br / (int)sizeof(int16_t);
            for (int i = 0; i + 1 < in_n; i += 2) {
                const int32_t al = abs_i32((int32_t)in_buf[i]);
                const int32_t ar = abs_i32((int32_t)in_buf[i + 1]);
                in_peak_l = al > in_peak_l ? al : in_peak_l;
                in_peak_r = ar > in_peak_r ? ar : in_peak_r;
            }
        }

        if (s_aud_mode == 1 && mic_ok) {
            const int frames = (int)br / (int)(sizeof(int16_t) * 2);
            int k = 0;
            while (k < frames && rec_pos < rec_total) {
                const int32_t l = (int32_t)in_buf[k * 2];
                const int32_t r = (int32_t)in_buf[k * 2 + 1];
                const int16_t mono = (int16_t)clamp_i32((l + r) / 2, -32768, 32767);
                if (s_aud_rec_mono) s_aud_rec_mono[rec_pos] = mono;
                rec_pos++;
                k++;
            }
        }

        if (mic_ok) {
            const int frames = wlen / (int)(sizeof(int16_t) * 2);
            wlen = frames * (int)(sizeof(int16_t) * 2);
            for (int i = 0; i < frames; i++) {
                const int32_t l = (int32_t)in_buf[i * 2];
                const int32_t r = (int32_t)in_buf[i * 2 + 1];
                const int32_t mono = (l + r) / 2;
                const int16_t s = (int16_t)clamp_i32(mono, -32768, 32767);
                out_buf[i * 2] = s;
                out_buf[i * 2 + 1] = s;
            }
            wbuf = out_buf;
        }

        if (!mic_ok) {
            in_peak_l = 0;
            in_peak_r = 0;
        }
        s_state.audio_peak_l = in_peak_l;
        s_state.audio_peak_r = in_peak_r;

        if (s_aud_mode == 1) {
            const TickType_t now_rec_tick = xTaskGetTickCount();
            const bool time_up = (int32_t)(now_rec_tick - rec_end_tick) >= 0;
            if (time_up || rec_pos >= rec_total) {
                while (rec_pos < rec_total) {
                    if (s_aud_rec_mono) s_aud_rec_mono[rec_pos] = 0;
                    rec_pos++;
                }
                s_aud_mode = 2;
                play_pos = 0;
                s_aud_play_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(3000);
            }
        }

        if (playing && wlen > 0) audio_service_write((void *)wbuf, wlen);
        if (wlen > 0) {
            int32_t minv = 32767;
            int32_t maxv = -32768;
            int32_t peak = 0;
            const int rn = wlen / (int)sizeof(int16_t);
            for (int i = 0; i < rn; i++) {
                const int32_t s = wbuf[i];
                minv = s < minv ? s : minv;
                maxv = s > maxv ? s : maxv;
                const int32_t a = abs_i32(s);
                peak = a > peak ? a : peak;
            }
            s_state.audio_min = minv;
            s_state.audio_max = maxv;
            s_state.audio_peak = peak;
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

const char *device_check_err_name(esp_err_t r)
{
    return (r == ESP_OK) ? "OK" : esp_err_to_name(r);
}
// 将带符号 32 位整数转换为带符号 32 位绝对值
static int32_t abs_i32(int32_t v)
{
    return v < 0 ? -v : v;
}
// 限制带符号 32 位整数在指定范围内
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    const int32_t v0 = v < lo ? lo : v;
    return v0 > hi ? hi : v0;
}

static bool rtc_time_seems_valid(const pcf85063a_datetime_t *t)
{
    if (!t) return false;

    bool ok = true;
    ok = ok && (t->year >= 2020);
    ok = ok && (t->year <= 2099);
    ok = ok && (t->month >= 1);
    ok = ok && (t->month <= 12);
    ok = ok && (t->day >= 1);
    ok = ok && (t->day <= 31);
    ok = ok && (t->hour <= 23);
    ok = ok && (t->min <= 59);
    ok = ok && (t->sec <= 59);
    return ok;
}

static uint8_t rtc_dow_from_ymd(int y, int m, int d)
{
    static const int t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    y = (m < 3) ? (y - 1) : y;
    const int w = (y + (y / 4) - (y / 100) + (y / 400) + t[m - 1] + d) % 7;
    return (uint8_t)w;
}

static uint8_t rtc_month_from_abbrev(const char *m3)
{
    if (!m3) return 0;
    if (m3[0] == 'J' && m3[1] == 'a' && m3[2] == 'n') return 1;
    if (m3[0] == 'F' && m3[1] == 'e' && m3[2] == 'b') return 2;
    if (m3[0] == 'M' && m3[1] == 'a' && m3[2] == 'r') return 3;
    if (m3[0] == 'A' && m3[1] == 'p' && m3[2] == 'r') return 4;
    if (m3[0] == 'M' && m3[1] == 'a' && m3[2] == 'y') return 5;
    if (m3[0] == 'J' && m3[1] == 'u' && m3[2] == 'n') return 6;
    if (m3[0] == 'J' && m3[1] == 'u' && m3[2] == 'l') return 7;
    if (m3[0] == 'A' && m3[1] == 'u' && m3[2] == 'g') return 8;
    if (m3[0] == 'S' && m3[1] == 'e' && m3[2] == 'p') return 9;
    if (m3[0] == 'O' && m3[1] == 'c' && m3[2] == 't') return 10;
    if (m3[0] == 'N' && m3[1] == 'o' && m3[2] == 'v') return 11;
    if (m3[0] == 'D' && m3[1] == 'e' && m3[2] == 'c') return 12;
    return 0;
}

static bool rtc_time_from_build(pcf85063a_datetime_t *out)
{
    if (!out) return false;

    const char *d = __DATE__;
    const char *t = __TIME__;
    const uint8_t month = rtc_month_from_abbrev(d);
    if (month == 0) return false;

    const int day = (d[4] == ' ') ? (d[5] - '0') : ((d[4] - '0') * 10 + (d[5] - '0'));
    const int year = (d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 + (d[10] - '0');

    const int hour = (t[0] - '0') * 10 + (t[1] - '0');
    const int min = (t[3] - '0') * 10 + (t[4] - '0');
    const int sec = (t[6] - '0') * 10 + (t[7] - '0');

    pcf85063a_datetime_t v = {
        .year = (uint16_t)year,
        .month = (uint8_t)month,
        .day = (uint8_t)day,
        .dotw = rtc_dow_from_ymd(year, month, day),
        .hour = (uint8_t)hour,
        .min = (uint8_t)min,
        .sec = (uint8_t)sec,
    };
    if (!rtc_time_seems_valid(&v)) return false;
    *out = v;
    return true;
}
/*
 * @brief 格式化带符号 32 位整数为 "±u.u" 格式
 * @param v100 带符号 32 位整数，单位为 0.01
 * @return 无
 */
static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//********************************************   硬件初始化任务   ********************************************//
static void hw_task(void *arg)
{
    (void)arg;

    s_state.shtc3_init_ret = s_only_aud ? ESP_ERR_NOT_SUPPORTED : sensor_service_init_shtc3();
    s_state.qmi_init_ret = s_only_aud ? ESP_ERR_NOT_SUPPORTED : sensor_service_init_qmi8658();
    s_state.rtc_init_ret = s_only_aud ? ESP_ERR_NOT_SUPPORTED : rtc_service_init();
    s_state.wifi_init_ret = s_only_aud ? ESP_ERR_NOT_SUPPORTED : ESP_FAIL;
    s_state.sd_init_ret = s_only_aud ? ESP_ERR_NOT_SUPPORTED : ESP_FAIL;

    s_state.sd_total_mb = 0;
    s_state.sd_free_mb = 0;
    s_state.sd_size_mb = 0;
    if (!s_only_aud) vTaskDelay(pdMS_TO_TICKS(300));

    if (s_state.rtc_init_ret == ESP_OK) {
        pcf85063a_datetime_t t;
        const esp_err_t r = rtc_service_get_time(&t);
        const bool ok = (r == ESP_OK) && rtc_time_seems_valid(&t);
        if (ok) {
            s_state.rtc_read_ret = ESP_OK;
            s_state.rtc_time = t;
        }
        if (!ok) {
            pcf85063a_datetime_t def;
            const bool got = rtc_time_from_build(&def);
            const esp_err_t sr = got ? rtc_service_set_time(def) : r;
            s_state.rtc_read_ret = sr;
            if (sr == ESP_OK) {
                s_state.rtc_time = def;
            }
        }
    }

    while (true) {
        const ui_page_t page = s_state.page;

        const bool want_wifi = (!s_only_aud) && (page == PAGE_WIFI);
        wifi_service_set_enabled(want_wifi);

        if (s_state.rtc_init_ret == ESP_OK) {
            const bool arm = (s_rtc_arm_req != 0);
            s_rtc_arm_req = arm ? 0 : s_rtc_arm_req;
            if (arm) rtc_service_arm_alarm_after(3);

            static uint32_t s_last_alarm_seq = 0;
            const uint32_t cur_seq = rtc_service_alarm_seq();
            const bool changed = (cur_seq != s_last_alarm_seq);
            s_last_alarm_seq = changed ? cur_seq : s_last_alarm_seq;
            if (changed && (s_state.page == PAGE_RTC)) rtc_service_arm_alarm_after(3);

            pcf85063a_datetime_t t;
            esp_err_t r = rtc_service_get_time(&t);
            s_state.rtc_read_ret = r;
            if (r == ESP_OK) s_state.rtc_time = t;
        }

        if (s_state.shtc3_init_ret == ESP_OK) {
            float t = NAN;
            float h = NAN;
            esp_err_t r = sensor_service_read_shtc3(&t, &h);
            if (r == ESP_OK) {
                s_state.temp_c = t;
                s_state.hum_rh = h;
            }
        }

        if (s_state.qmi_init_ret == ESP_OK) {
            float ax = NAN, ay = NAN, az = NAN, gx = NAN, gy = NAN, gz = NAN;
            esp_err_t r = sensor_service_read_qmi(&ax, &ay, &az, &gx, &gy, &gz);
            if (r == ESP_OK) {
                s_state.ax = ax;
                s_state.ay = ay;
                s_state.az = az;
                s_state.gx = gx;
                s_state.gy = gy;
                s_state.gz = gz;
            }
        }

        if (want_wifi) {
            wifi_service_status_t st;
            esp_err_t r = wifi_service_get_status(&st);
            s_state.wifi_init_ret = r;
            if (r == ESP_OK) {
                s_state.wifi_sta_configured = st.sta_configured;
                s_state.wifi_sta_connected = st.sta_connected;
                s_state.wifi_sta_ip = st.sta_ip;
                s_state.wifi_sta_rssi = st.sta_rssi;
                s_state.wifi_ap_on = st.ap_on;
                s_state.wifi_ap_clients = st.ap_clients;
                s_state.wifi_ap_ip = st.ap_ip;
            }
        }

        if (!s_only_aud) {
            size_t total_mb = 0;
            size_t free_mb = 0;
            const esp_err_t r = sdcard_service_get_capacity_mb(&total_mb, &free_mb);
            s_state.sd_init_ret = r;
            if (r == ESP_OK) {
                s_state.sd_total_mb = total_mb;
                s_state.sd_free_mb = free_mb;
                s_state.sd_size_mb = total_mb;
            }
            if (r != ESP_OK) {
                s_state.sd_total_mb = 0;
                s_state.sd_free_mb = 0;
                s_state.sd_size_mb = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(s_only_aud ? 20 : 500));
    }
}

/*
 * @brief 启动设备检查任务
 * @param 无
 * @return 无
 */
void device_check_start(void)
{
    esp_err_t r = bsp_init_display();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "LVGL init failed: %s", esp_err_to_name(r));
        return;
    }

    device_check_ui_show_rgb_splash(1000);

    s_state.page = s_only_aud ? PAGE_AUDIO : PAGE_SHTC3;
    s_state.audio_out_vol = s_aud_out_vol;
    s_state.audio_mode = s_aud_mode;
    s_state.audio_playing = false;

    device_check_ui_init_screen();

    xTaskCreate(audio_task, "aud", 4096, NULL, tskIDLE_PRIORITY + 4, NULL);

    {
        const key_service_config_t cfg = {
            .gpio_num = GPIO_NUM_0,
            .active_level = 0,
            .poll_interval_ms = 10,
            .debounce_ms = 30,
            .double_click_ms = 350,
            .long_press_ms = 800,
            .long_repeat_ms = 1000,
        };
        const esp_err_t key_r = key_service_start(&cfg, device_check_key_cb, NULL);
        if (key_r != ESP_OK) ESP_LOGE(TAG, "key init failed: %s", esp_err_to_name(key_r));
    }

    xTaskCreate(hw_task, "hw", 4096, NULL, tskIDLE_PRIORITY + 2, NULL);

    device_check_ui_install_timer(200);
    device_check_ui_update();

    if (!s_lvgl_task) {
        BaseType_t ok = xTaskCreate(lvgl_task, "lvgl", 6144, NULL, tskIDLE_PRIORITY + 1, &s_lvgl_task);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "create lvgl task failed");
        }
    }
}

