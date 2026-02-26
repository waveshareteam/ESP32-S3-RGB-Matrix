#pragma once

#include "esp_err.h"

#include <stdbool.h>

esp_err_t sensor_service_init_shtc3(void);
esp_err_t sensor_service_init_qmi8658(void);

esp_err_t sensor_service_read_shtc3(float *temp_c, float *hum_rh);
esp_err_t sensor_service_read_qmi(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);

