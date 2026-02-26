#include "sensor_service.h"

#include "extra_bsp.h"

#include "esp_timer.h"

static bool s_shtc3_inited = false;
static bool s_qmi_inited = false;

static float s_cache_temp = 0;
static float s_cache_hum = 0;
static int64_t s_cache_shtc3_us = 0;

static float s_cache_ax = 0;
static float s_cache_ay = 0;
static float s_cache_az = 0;
static float s_cache_gx = 0;
static float s_cache_gy = 0;
static float s_cache_gz = 0;
static int64_t s_cache_qmi_us = 0;

static void sensor_service_reset_cache(void)
{
    s_cache_shtc3_us = 0;
    s_cache_qmi_us = 0;
}

esp_err_t sensor_service_init_shtc3(void)
{
    if (s_shtc3_inited) return ESP_OK;
    esp_err_t r = bsp_init_shtc3();
    if (r != ESP_OK) return r;
    s_shtc3_inited = true;
    sensor_service_reset_cache();
    return ESP_OK;
}

esp_err_t sensor_service_init_qmi8658(void)
{
    if (s_qmi_inited) return ESP_OK;
    esp_err_t r = bsp_init_qmi8658();
    if (r != ESP_OK) return r;
    s_qmi_inited = true;
    sensor_service_reset_cache();
    return ESP_OK;
}

esp_err_t sensor_service_read_shtc3(float *temp_c, float *hum_rh)
{
    if (!temp_c || !hum_rh) return ESP_ERR_INVALID_ARG;

    if (!s_shtc3_inited) {
        esp_err_t ir = sensor_service_init_shtc3();
        if (ir != ESP_OK) return ir;
    }

    const int64_t now_us = esp_timer_get_time();
    const bool cache_valid = (s_cache_shtc3_us != 0) && ((now_us - s_cache_shtc3_us) < 200 * 1000);
    if (cache_valid) {
        *temp_c = s_cache_temp;
        *hum_rh = s_cache_hum;
        return ESP_OK;
    }

    float t = 0;
    float h = 0;
    esp_err_t r = bsp_read_shtc3(&t, &h);
    if (r != ESP_OK) return r;

    s_cache_temp = t;
    s_cache_hum = h;
    s_cache_shtc3_us = now_us;
    *temp_c = t;
    *hum_rh = h;
    return ESP_OK;
}

esp_err_t sensor_service_read_qmi(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    if (!ax || !ay || !az || !gx || !gy || !gz) return ESP_ERR_INVALID_ARG;

    if (!s_qmi_inited) {
        esp_err_t ir = sensor_service_init_qmi8658();
        if (ir != ESP_OK) return ir;
    }

    const int64_t now_us = esp_timer_get_time();
    const bool cache_valid = (s_cache_qmi_us != 0) && ((now_us - s_cache_qmi_us) < 50 * 1000);
    if (cache_valid) {
        *ax = s_cache_ax;
        *ay = s_cache_ay;
        *az = s_cache_az;
        *gx = s_cache_gx;
        *gy = s_cache_gy;
        *gz = s_cache_gz;
        return ESP_OK;
    }

    float a0 = 0;
    float a1 = 0;
    float a2 = 0;
    float g0 = 0;
    float g1 = 0;
    float g2 = 0;
    esp_err_t r = bsp_read_qmi(&a0, &a1, &a2, &g0, &g1, &g2);
    if (r != ESP_OK) return r;

    s_cache_ax = a0;
    s_cache_ay = a1;
    s_cache_az = a2;
    s_cache_gx = g0;
    s_cache_gy = g1;
    s_cache_gz = g2;
    s_cache_qmi_us = now_us;

    *ax = a0;
    *ay = a1;
    *az = a2;
    *gx = g0;
    *gy = g1;
    *gz = g2;
    return ESP_OK;
}
