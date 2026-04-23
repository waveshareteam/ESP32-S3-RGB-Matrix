#pragma once

#include "esp_err.h"
#include <stddef.h>

esp_err_t middle_sdcard_init(void); // Initialize SD card service

esp_err_t middle_sdcard_deinit(void); // Deinitialize SD card service and release resources

/*
 * @brief Get total and free capacity of the SD card (unit: MB)
 * @param total_mb Total capacity (unit: MB)
 * @param free_mb Free capacity (unit: MB)
 * @return esp_err_t Error code
 */
esp_err_t middle_sdcard_get_capacity_mb(size_t *total_mb, size_t *free_mb);

/*
 * @brief Get SD card size (unit: MB)
 * @param size_mb SD card size (unit: MB)
 * @return esp_err_t Error code
 */
esp_err_t middle_sdcard_get_size_mb(size_t *size_mb);

const char *middle_sdcard_root(void); // Get SD card root directory path

