#include "middle_sensor.h"
#include "bsp/esp32_s3_matrix.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "qmi8658.h"
#include "shtc3.h"
#include <stdint.h>

static const uint32_t SHTC3_I2C_SPEED_HZ = 40000;
static const uint32_t SHTC3_I2C_SCL_WAIT_US = 20000;
static const int64_t SHTC3_CACHE_WINDOW_US = 200 * 1000;
static const char *TAG = "middle";

static i2c_master_dev_handle_t shtc3_dev = NULL;
static int64_t cache_shtc3_us = 0;
static bool shtc3_inited = false;
static float cache_temp = 0;
static float cache_hum = 0;

static int64_t cache_qmi_us = 0;
static qmi8658_dev_t qmi_dev;
static bool qmi_inited = false;

static float cache_ax = 0;
static float cache_ay = 0;
static float cache_az = 0;
static float cache_gx = 0;
static float cache_gy = 0;
static float cache_gz = 0;

static void reset_cache(void)
{
    cache_shtc3_us = 0;
    cache_qmi_us = 0;
}

static esp_err_t middle_init_shtc3_dev(bool recreate)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp_i2c_init failed");

    if (shtc3_dev && !recreate) return ESP_OK;

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;

    if (shtc3_dev) {
        i2c_master_bus_rm_device(shtc3_dev);
        shtc3_dev = NULL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_I2C_ADDR,
        .scl_speed_hz = SHTC3_I2C_SPEED_HZ,
        .scl_wait_us = SHTC3_I2C_SCL_WAIT_US,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &shtc3_dev), TAG, "i2c_master_bus_add_device failed");
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, SHTC3_I2C_ADDR, 20), TAG, "i2c_master_probe failed");
    return ESP_OK;
}

esp_err_t middle_init_shtc3(void)
{
    if (shtc3_inited) return ESP_OK;
    ESP_RETURN_ON_ERROR(middle_init_shtc3_dev(false), TAG, "middle_init_shtc3_dev failed");

    shtc3_inited = true;
    reset_cache();
    return ESP_OK;
}

esp_err_t middle_init_qmi8658(void)
{
    if (qmi_inited) return ESP_OK;

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp_i2c_init failed");
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;

    ESP_RETURN_ON_ERROR(qmi8658_init(&qmi_dev, bus, QMI8658_ADDRESS_HIGH), TAG, "qmi8658_init failed");

    qmi8658_set_accel_unit_mps2(&qmi_dev, true);
    qmi8658_set_gyro_unit_rads(&qmi_dev, true);
    qmi8658_set_display_precision(&qmi_dev, 4);
    ESP_RETURN_ON_ERROR(qmi8658_enable_sensors(&qmi_dev, QMI8658_ENABLE_ACCEL | QMI8658_ENABLE_GYRO),
                        TAG, "qmi8658_enable_sensors failed");

    qmi_inited = true;
    reset_cache();
    return ESP_OK;
}

esp_err_t middle_read_shtc3(float *temp_c, float *hum_rh)
{
    if (!temp_c || !hum_rh) return ESP_ERR_INVALID_ARG;
    if (!shtc3_inited) return ESP_ERR_INVALID_STATE;

    const int64_t now_us = esp_timer_get_time();
    const bool cache_valid = (cache_shtc3_us != 0) && ((now_us - cache_shtc3_us) < SHTC3_CACHE_WINDOW_US);
    if (cache_valid) {
        *temp_c = cache_temp;
        *hum_rh = cache_hum;
        return ESP_OK;
    }

    float t = 0;
    float h = 0;
    ESP_RETURN_ON_ERROR(shtc3_get_th(shtc3_dev, SHTC3_REG_T_CSE_NM, &t, &h), TAG, "shtc3_get_th failed");

    cache_temp = t;
    cache_hum = h;
    cache_shtc3_us = now_us;
    *temp_c = t;
    *hum_rh = h;
    return ESP_OK;
}

esp_err_t middle_read_qmi(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    if (!ax || !ay || !az || !gx || !gy || !gz) return ESP_ERR_INVALID_ARG;
    if (!qmi_inited) return ESP_ERR_INVALID_STATE;

    const int64_t now_us = esp_timer_get_time();
    const bool cache_valid = (cache_qmi_us != 0) && ((now_us - cache_qmi_us) < 50 * 1000);
    if (cache_valid) {
        *ax = cache_ax;
        *ay = cache_ay;
        *az = cache_az;
        *gx = cache_gx;
        *gy = cache_gy;
        *gz = cache_gz;
        return ESP_OK;
    }

    float a0 = 0;
    float a1 = 0;
    float a2 = 0;
    float g0 = 0;
    float g1 = 0;
    float g2 = 0;
    
    ESP_RETURN_ON_ERROR(qmi8658_read_accel(&qmi_dev, &a0, &a1, &a2), TAG, "qmi8658_read_accel failed");
    ESP_RETURN_ON_ERROR(qmi8658_read_gyro(&qmi_dev, &g0, &g1, &g2), TAG, "qmi8658_read_gyro failed");

    cache_ax = a0;
    cache_ay = a1;
    cache_az = a2;
    cache_gx = g0;
    cache_gy = g1;
    cache_gz = g2;
    cache_qmi_us = now_us;

    *ax = a0;
    *ay = a1;
    *az = a2;
    *gx = g0;
    *gy = g1;
    *gz = g2;
    return ESP_OK;
}
