#pragma once

#include "esp_err.h"

#include <stddef.h>

esp_err_t sdcard_service_init(void);
esp_err_t sdcard_service_deinit(void);

esp_err_t sdcard_service_get_capacity_mb(size_t *total_mb, size_t *free_mb);
esp_err_t sdcard_service_get_size_mb(size_t *size_mb);

const char *sdcard_service_root(void);

