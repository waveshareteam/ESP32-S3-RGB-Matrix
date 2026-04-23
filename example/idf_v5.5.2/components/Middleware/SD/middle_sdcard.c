#include "middle_sdcard.h"
#include "bsp/esp32_s3_matrix.h"
#include "esp_check.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <stdbool.h>

static const char *TAG = "middle_sdcard";
static bool sdcard_inited = false;

static esp_err_t sdcard_query_capacity_mb(size_t *total_mb, size_t *free_mb)
{
    uint64_t tot_bytes = 0;
    uint64_t free_bytes = 0;
    if (!total_mb || !free_mb) return ESP_ERR_INVALID_ARG;
    sdmmc_card_t *card = bsp_sdcard_get_handle();
    if (!card) return ESP_ERR_INVALID_STATE;
    ESP_RETURN_ON_ERROR(sdmmc_get_status(card), TAG, "sdmmc_get_status failed");
    ESP_RETURN_ON_ERROR(esp_vfs_fat_info(BSP_SD_MOUNT_POINT, &tot_bytes, &free_bytes), TAG, "esp_vfs_fat_info failed");
    *total_mb = (size_t)(tot_bytes / (1024ULL * 1024ULL));
    *free_mb = (size_t)(free_bytes / (1024ULL * 1024ULL));
    return ESP_OK;
}

static esp_err_t sdcard_query_size_mb(size_t *size_mb)
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

esp_err_t middle_sdcard_init(void)
{
    if (sdcard_inited) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_sdcard_mount(), TAG, "bsp_sdcard_mount failed");
    sdcard_inited = true;
    return ESP_OK;
}

esp_err_t middle_sdcard_deinit(void)
{
    if (!sdcard_inited) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_sdcard_unmount(), TAG, "bsp_sdcard_unmount failed");
    sdcard_inited = false;
    return ESP_OK;
}

esp_err_t middle_sdcard_get_capacity_mb(size_t *total_mb, size_t *free_mb)
{
    if (!total_mb || !free_mb) return ESP_ERR_INVALID_ARG;
    if (!sdcard_inited) return ESP_ERR_INVALID_STATE;
    return sdcard_query_capacity_mb(total_mb, free_mb);
}

esp_err_t middle_sdcard_get_size_mb(size_t *size_mb)
{
    if (!size_mb) return ESP_ERR_INVALID_ARG;
    if (!sdcard_inited) return ESP_ERR_INVALID_STATE;
    return sdcard_query_size_mb(size_mb);
}

const char *middle_sdcard_root(void)
{
    return BSP_SD_MOUNT_POINT;
}
