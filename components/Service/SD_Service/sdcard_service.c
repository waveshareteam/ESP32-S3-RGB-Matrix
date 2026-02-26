#include "sdcard_service.h"
#include "bsp/esp32_s3_matrix.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <stdbool.h>

static bool s_sdcard_inited = false;
static uint32_t s_retry_ms = 500;
static int64_t s_next_try_us = 0;
static esp_err_t s_last_err = ESP_ERR_INVALID_STATE;

static size_t s_cache_total_mb = 0;
static size_t s_cache_free_mb = 0;
static int64_t s_cache_ts_us = 0;

static esp_err_t sdcard_get_capacity_mb(size_t *total_mb, size_t *free_mb)
{
    if (!total_mb || !free_mb) return ESP_ERR_INVALID_ARG;

    sdmmc_card_t *card = bsp_sdcard_get_handle();
    if (!card) return ESP_ERR_INVALID_STATE;

    esp_err_t sr = sdmmc_get_status(card);
    if (sr != ESP_OK) return sr;

    uint64_t tot_bytes = 0;
    uint64_t free_bytes = 0;
    esp_err_t r = esp_vfs_fat_info(BSP_SD_MOUNT_POINT, &tot_bytes, &free_bytes);
    if (r != ESP_OK) return r;

    *total_mb = (size_t)(tot_bytes / (1024ULL * 1024ULL));
    *free_mb = (size_t)(free_bytes / (1024ULL * 1024ULL));
    return ESP_OK;
}

static esp_err_t sdcard_get_size_mb(size_t *size_mb)
{
    if (!size_mb) return ESP_ERR_INVALID_ARG;

    sdmmc_card_t *card = bsp_sdcard_get_handle();
    if (!card) return ESP_ERR_INVALID_STATE;

    uint64_t raw = (uint64_t)card->csd.capacity;
    uint64_t mb_bytes = raw / (1024ULL * 1024ULL);
    uint64_t mb_sectors = (raw * 512ULL) / (1024ULL * 1024ULL);
    *size_mb = (size_t)((mb_bytes > mb_sectors) ? mb_bytes : mb_sectors);
    return ESP_OK;
}

static void sdcard_service_reset_retry(void)
{
    s_retry_ms = 500;
    s_next_try_us = 0;
    s_last_err = ESP_ERR_INVALID_STATE;
}

static void sdcard_service_reset_cache(void)
{
    s_cache_total_mb = 0;
    s_cache_free_mb = 0;
    s_cache_ts_us = 0;
}

static void sdcard_service_schedule_retry(int64_t now_us, esp_err_t err)
{
    s_last_err = err;

    const uint32_t next_ms = (s_retry_ms < 10000U) ? (s_retry_ms * 2U) : 10000U;
    s_retry_ms = next_ms;
    s_next_try_us = now_us + (int64_t)s_retry_ms * 1000;
}

static esp_err_t sdcard_service_try_init(int64_t now_us)
{
    if (s_sdcard_inited) return ESP_OK;

    const bool can_try = (s_next_try_us == 0) || (now_us >= s_next_try_us);
    if (!can_try) return s_last_err;

    esp_err_t r = bsp_sdcard_mount();
    if (r == ESP_OK) {
        s_sdcard_inited = true;
        s_retry_ms = 500;
        s_next_try_us = 0;
        s_last_err = ESP_OK;
        sdcard_service_reset_cache();
        return ESP_OK;
    }

    sdcard_service_schedule_retry(now_us, r);
    return r;
}

esp_err_t sdcard_service_init(void)
{
    if (s_sdcard_inited) return ESP_OK;

    const int64_t now_us = esp_timer_get_time();
    esp_err_t r = bsp_sdcard_mount();
    if (r == ESP_OK) {
        s_sdcard_inited = true;
        sdcard_service_reset_retry();
        s_last_err = ESP_OK;
        sdcard_service_reset_cache();
        return ESP_OK;
    }

    sdcard_service_schedule_retry(now_us, r);
    return r;
}

esp_err_t sdcard_service_deinit(void)
{
    if (!s_sdcard_inited) return ESP_OK;

    esp_err_t r = bsp_sdcard_unmount();
    if (r != ESP_OK) return r;

    s_sdcard_inited = false;
    sdcard_service_reset_retry();
    sdcard_service_reset_cache();
    return ESP_OK;
}

esp_err_t sdcard_service_get_capacity_mb(size_t *total_mb, size_t *free_mb)
{
    if (!total_mb || !free_mb) return ESP_ERR_INVALID_ARG;

    const int64_t now_us = esp_timer_get_time();
    const bool cache_valid = (s_cache_ts_us != 0) && ((now_us - s_cache_ts_us) < 1000 * 1000);
    if (cache_valid) {
        *total_mb = s_cache_total_mb;
        *free_mb = s_cache_free_mb;
        return ESP_OK;
    }

    esp_err_t r = sdcard_service_try_init(now_us);
    if (r != ESP_OK) return r;

    size_t t = 0;
    size_t f = 0;
    r = sdcard_get_capacity_mb(&t, &f);
    if (r == ESP_OK) {
        s_cache_total_mb = t;
        s_cache_free_mb = f;
        s_cache_ts_us = now_us;
        *total_mb = t;
        *free_mb = f;
        return ESP_OK;
    }

    bsp_sdcard_unmount();
    s_sdcard_inited = false;
    sdcard_service_schedule_retry(now_us, r);
    sdcard_service_reset_cache();
    return r;
}

esp_err_t sdcard_service_get_size_mb(size_t *size_mb)
{
    if (!size_mb) return ESP_ERR_INVALID_ARG;

    const int64_t now_us = esp_timer_get_time();
    esp_err_t r = sdcard_service_try_init(now_us);
    if (r != ESP_OK) return r;

    r = sdcard_get_size_mb(size_mb);
    if (r == ESP_OK) return ESP_OK;

    bsp_sdcard_unmount();
    s_sdcard_inited = false;
    sdcard_service_schedule_retry(now_us, r);
    sdcard_service_reset_cache();
    return r;
}

const char *sdcard_service_root(void)
{
    return BSP_SD_MOUNT_POINT;
}
