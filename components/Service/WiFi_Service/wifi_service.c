#include "wifi_service.h"

#include "extra_bsp.h"

#include "esp_timer.h"

static bool s_enabled = false;
static bool s_running = false;

static uint32_t s_retry_ms = 500;
static int64_t s_next_try_us = 0;
static esp_err_t s_last_err = ESP_ERR_INVALID_STATE;

static wifi_service_status_t s_cache;
static int64_t s_cache_ts_us = 0;

static void wifi_service_reset_retry(void)
{
    s_retry_ms = 500;
    s_next_try_us = 0;
    s_last_err = ESP_ERR_INVALID_STATE;
}

static void wifi_service_reset_cache(void)
{
    s_cache = (wifi_service_status_t){ 0 };
    s_cache_ts_us = 0;
}

static void wifi_service_schedule_retry(int64_t now_us, esp_err_t err)
{
    s_last_err = err;
    const uint32_t next_ms = (s_retry_ms < 10000U) ? (s_retry_ms * 2U) : 10000U;
    s_retry_ms = next_ms;
    s_next_try_us = now_us + (int64_t)s_retry_ms * 1000;
}

static esp_err_t wifi_service_try_start(int64_t now_us)
{
    if (!s_enabled) return ESP_ERR_INVALID_STATE;
    if (s_running) return ESP_OK;

    const bool can_try = (s_next_try_us == 0) || (now_us >= s_next_try_us);
    if (!can_try) return s_last_err;

    esp_err_t r = bsp_init_wifi_apsta();
    if (r == ESP_OK) {
        s_running = true;
        wifi_service_reset_retry();
        s_last_err = ESP_OK;
        wifi_service_reset_cache();
        return ESP_OK;
    }

    wifi_service_schedule_retry(now_us, r);
    return r;
}

static void wifi_service_apply_enable(bool enable)
{
    if (enable) {
        s_enabled = true;
        return;
    }

    s_enabled = false;
    if (s_running) bsp_wifi_stop();
    s_running = false;
    wifi_service_reset_retry();
    wifi_service_reset_cache();
}

void wifi_service_set_enabled(bool enable)
{
    if (enable == s_enabled) return;
    wifi_service_apply_enable(enable);
}

esp_err_t wifi_service_get_status(wifi_service_status_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    *out = (wifi_service_status_t){
        .last_err = ESP_ERR_INVALID_STATE,
    };

    if (!s_enabled) return ESP_ERR_INVALID_STATE;

    const int64_t now_us = esp_timer_get_time();
    esp_err_t r = wifi_service_try_start(now_us);
    if (r != ESP_OK) {
        out->last_err = r;
        return r;
    }

    const bool cache_valid = (s_cache_ts_us != 0) && ((now_us - s_cache_ts_us) < 500 * 1000);
    if (cache_valid) {
        *out = s_cache;
        return ESP_OK;
    }

    wifi_service_status_t cur = { 0 };
    r = bsp_wifi_get_status(&cur.sta_configured,
                                           &cur.sta_connected,
                                           &cur.sta_ip,
                                           &cur.sta_rssi,
                                           &cur.ap_on,
                                           &cur.ap_clients,
                                           &cur.ap_ip);
    cur.last_err = r;
    if (r == ESP_OK) {
        s_cache = cur;
        s_cache_ts_us = now_us;
        *out = cur;
        return ESP_OK;
    }

    bsp_wifi_stop();
    s_running = false;
    wifi_service_schedule_retry(now_us, r);
    wifi_service_reset_cache();
    out->last_err = r;
    return r;
}
