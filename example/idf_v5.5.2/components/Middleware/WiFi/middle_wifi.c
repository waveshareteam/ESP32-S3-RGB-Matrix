#include "middle_wifi.h"
#include "bsp/esp32_s3_matrix.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "middle_wifi";

static char sta_ssid[33] = {0};
static char sta_pass[65] = {0};
static bool inited = false;
static TaskHandle_t wifi_task_handle = NULL;

static middle_wifi_status_t cache = {
    .last_err = ESP_ERR_INVALID_STATE,
};

static void wifi_reset_cache(void)
{
    cache = (middle_wifi_status_t){
        .last_err = ESP_ERR_INVALID_STATE,
    };
}

static esp_err_t wifi_info_refresh(void)
{
    middle_wifi_status_t cur = {0};
    esp_err_t r = bsp_wifi_get_status(&cur.sta_configured,
                                      &cur.sta_connected,
                                      &cur.sta_ip,
                                      &cur.sta_rssi,
                                      &cur.ap_on,
                                      &cur.ap_clients,
                                      &cur.ap_ip);
    cur.last_err = r;
    cache = cur;
    return r;
}

static void wifi_refresh_task(void *arg)
{
    while (true) {
        (void)wifi_info_refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t middle_wifi_get_status(middle_wifi_status_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = cache;
    if (!inited) return ESP_ERR_INVALID_STATE;
    return cache.last_err;
}

void middle_wifi_set_sta_config(const char *ssid, const char *password)
{
    sta_ssid[0] = '\0';
    sta_pass[0] = '\0';
    if (ssid) {
        strncpy(sta_ssid, ssid, sizeof(sta_ssid) - 1);
    }
    if (password) {
        strncpy(sta_pass, password, sizeof(sta_pass) - 1);
    }
    wifi_reset_cache();
}

esp_err_t middle_wifi_init(void)
{
    if (inited) return ESP_OK;

    const char *ssid = sta_ssid[0] ? sta_ssid : NULL;
    const char *pass = sta_pass[0] ? sta_pass : NULL;
    wifi_reset_cache();
    esp_err_t r = bsp_init_wifi_apsta(ssid, pass);
    cache.last_err = r;
    ESP_RETURN_ON_ERROR(r, TAG, "bsp_init_wifi_apsta failed");

    inited = true;
    wifi_info_refresh();
    BaseType_t task_ok = xTaskCreate(wifi_refresh_task, "wifi_refresh_task", 3072, NULL,
                                     tskIDLE_PRIORITY + 1, &wifi_task_handle);
    if (task_ok == pdPASS) return ESP_OK;

    wifi_task_handle = NULL;
    inited = false;
    bsp_wifi_stop();
    cache.last_err = ESP_FAIL;
    return ESP_FAIL;
}
