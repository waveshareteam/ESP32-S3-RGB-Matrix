#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t middle_init_shtc3(void); // Initialize SHTC3 sensor

esp_err_t middle_init_qmi8658(void); // Initialize QMI8658 sensor

/*
 * @brief Read SHTC3 sensor data (temperature and humidity)
 * @param temp_c Temperature (unit: °C)
 * @param hum_rh Humidity (unit: %RH)
 * @return esp_err_t Error code
 */
esp_err_t middle_read_shtc3(float *temp_c, float *hum_rh);

/*
 * @brief Read QMI8658 sensor data (accelerometer and gyroscope)
 * @param ax Acceleration X-axis
 * @param ay Acceleration Y-axis
 * @param az Acceleration Z-axis
 * @param gx Gyroscope X-axis
 * @param gy Gyroscope Y-axis
 * @param gz Gyroscope Z-axis
 * @return esp_err_t Error code
 */
esp_err_t middle_read_qmi(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);

