#include "extra_bsp.h"
#include "bsp/esp32_s3_matrix.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "pcf85063a.h"
#include "qmi8658.h"
#include <stdio.h>
#include <string.h>

static const char *TAG_QMI = "QMI8658";
static const char *TAG_RTC = "PCF85063A";
static const char *TAG_WIFI = "WIFI";

static i2c_master_dev_handle_t s_dev_shtc3 = NULL;
static int s_shtc3_timeout_ms = 1000;

static bool s_qmi_inited = false;
static qmi8658_dev_t s_qmi_dev;

static bool s_rtc_inited = false;
static pcf85063a_dev_t s_rtc_dev;
static bool s_gpio_isr_service_inited = false;
static void (*s_rtc_int_cb)(void *) = NULL;
static void *s_rtc_int_cb_arg = NULL;

static bool s_nvs_inited = false;
static bool s_wifi_inited = false;
static esp_netif_t *s_netif_sta = NULL;
static esp_netif_t *s_netif_ap = NULL;
static int64_t s_sta_next_connect_us = 0;

static void IRAM_ATTR rtc_int_isr(void *arg)
{
    (void)arg;
    void (*cb)(void *) = s_rtc_int_cb;
    if (!cb) return;
    cb(s_rtc_int_cb_arg);
}

static esp_err_t qmi8658_init_auto(i2c_master_bus_handle_t bus)
{
    if (!bus) return ESP_ERR_INVALID_ARG;

    memset(&s_qmi_dev, 0, sizeof(s_qmi_dev));

    const uint8_t addrs[] = {
        BSP_QMI8658_ADDR_H,
        BSP_QMI8658_ADDR_L,
    };

    for (size_t i = 0; i < (sizeof(addrs) / sizeof(addrs[0])); i++) {
        esp_err_t r = qmi8658_init(&s_qmi_dev, bus, addrs[i]);
        if (r == ESP_OK) return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t qmi8658_read6(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    if (!ax || !ay || !az || !gx || !gy || !gz) return ESP_ERR_INVALID_ARG;
    if (!s_qmi_inited) return ESP_ERR_INVALID_STATE;

    qmi8658_data_t d;
    ESP_RETURN_ON_ERROR(qmi8658_read_sensor_data(&s_qmi_dev, &d), TAG_QMI, "read data");

    const bool accel_mps2 = qmi8658_is_accel_unit_mps2(&s_qmi_dev);
    const float acc_div = accel_mps2 ? ONE_G : 1000.0f;
    *ax = d.accelX / acc_div;
    *ay = d.accelY / acc_div;
    *az = d.accelZ / acc_div;

    *gx = d.gyroX;
    *gy = d.gyroY;
    *gz = d.gyroZ;
    return ESP_OK;
}

static esp_err_t shtc3_write_cmd16(uint16_t cmd_val)
{
    if (!s_dev_shtc3) return ESP_ERR_INVALID_STATE;
    uint8_t tx[2] = { (uint8_t)(cmd_val >> 8), (uint8_t)(cmd_val & 0xFF) };
    return i2c_master_transmit(s_dev_shtc3, tx, sizeof(tx), s_shtc3_timeout_ms);
}

static uint8_t shtc3_calc_crc(uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (size_t j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t shtc3_init(i2c_master_bus_handle_t bus, int scl_speed_hz, int timeout_ms)
{
    if (!bus) return ESP_ERR_INVALID_ARG;
    if (scl_speed_hz <= 0) return ESP_ERR_INVALID_ARG;
    if (timeout_ms <= 0) return ESP_ERR_INVALID_ARG;

    s_shtc3_timeout_ms = timeout_ms;
    if (!s_dev_shtc3) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = BSP_SHTC3_ADDR,
            .scl_speed_hz = scl_speed_hz,
        };
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev_shtc3), "SHTC3", "add dev");
    }

    esp_err_t ret = shtc3_write_cmd16(BSP_SHTC3_CMD_WAKEUP);
    vTaskDelay(pdMS_TO_TICKS(10));
    return ret;
}

static esp_err_t shtc3_read(float *temp, float *hum)
{
    if (!temp || !hum) return ESP_ERR_INVALID_ARG;
    if (!s_dev_shtc3) return ESP_ERR_INVALID_STATE;

    shtc3_write_cmd16(BSP_SHTC3_CMD_WAKEUP);
    vTaskDelay(pdMS_TO_TICKS(10));

    esp_err_t ret = shtc3_write_cmd16(BSP_SHTC3_CMD_MEASURE_NPM);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6];
    ret = i2c_master_receive(s_dev_shtc3, data, sizeof(data), s_shtc3_timeout_ms);
    shtc3_write_cmd16(BSP_SHTC3_CMD_SLEEP);
    if (ret != ESP_OK) return ret;

    if (shtc3_calc_crc(&data[0], 2) != data[2]) return ESP_ERR_INVALID_CRC;
    if (shtc3_calc_crc(&data[3], 2) != data[5]) return ESP_ERR_INVALID_CRC;

    uint16_t t_raw = (uint16_t)((data[0] << 8) | data[1]);
    uint16_t h_raw = (uint16_t)((data[3] << 8) | data[4]);

    *temp = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    *hum = 100.0f * ((float)h_raw / 65535.0f);

    return ESP_OK;
}

static esp_err_t nvs_ensure_inited(void)
{
    if (s_nvs_inited) return ESP_OK;

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        esp_err_t er = nvs_flash_erase();
        if (er != ESP_OK) return er;
        r = nvs_flash_init();
    }
    if (r != ESP_OK) return r;
    s_nvs_inited = true;
    return ESP_OK;
}

static void wifi_fill_open_ap_cfg(wifi_config_t *ap_cfg, const char *ssid)
{
    memset(ap_cfg, 0, sizeof(*ap_cfg));
    snprintf((char *)ap_cfg->ap.ssid, sizeof(ap_cfg->ap.ssid), "%s", ssid);
    ap_cfg->ap.ssid_len = (uint8_t)strlen(ssid);
    ap_cfg->ap.channel = 6;
    ap_cfg->ap.authmode = WIFI_AUTH_OPEN;
    ap_cfg->ap.max_connection = 4;
}

static esp_err_t wifi_start_apsta_with_ap_cfg(wifi_config_t *ap_cfg, esp_err_t allow_start_err)
{
    esp_err_t r = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (r != ESP_OK) return r;

    r = esp_wifi_set_config(WIFI_IF_AP, ap_cfg);
    if (r != ESP_OK) return r;

    r = esp_wifi_start();
    if (r == ESP_OK || r == allow_start_err) return ESP_OK;
    return r;
}

esp_err_t bsp_init_shtc3(void)
{
    esp_err_t r = bsp_i2c_init();
    if (r != ESP_OK) return r;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;
    return shtc3_init(bus, BSP_I2C_FREQ_HZ, BSP_I2C_TIMEOUT_MS);
}

esp_err_t bsp_init_qmi8658(void)
{
    esp_err_t r = bsp_i2c_init();
    if (r != ESP_OK) return r;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;

    if (s_qmi_inited) return ESP_OK;
    r = qmi8658_init_auto(bus);
    if (r != ESP_OK) return r;

    qmi8658_set_accel_unit_mg(&s_qmi_dev, true);
    qmi8658_set_gyro_unit_dps(&s_qmi_dev, true);

    s_qmi_inited = true;
    return ESP_OK;
}

esp_err_t bsp_init_pcf85063a(void)
{
    if (s_rtc_inited) return ESP_OK;

    esp_err_t r = bsp_i2c_init();
    if (r != ESP_OK) return r;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;

    memset(&s_rtc_dev, 0, sizeof(s_rtc_dev));
    r = pcf85063a_init(&s_rtc_dev, bus, BSP_PCF85063A_ADDR);
    if (r != ESP_OK) return r;

    s_rtc_inited = true;
    ESP_LOGI(TAG_RTC, "inited at 0x%02X", (unsigned)BSP_PCF85063A_ADDR);
    return ESP_OK;
}

esp_err_t bsp_read_shtc3(float *temp_c, float *hum_rh)
{
    return shtc3_read(temp_c, hum_rh);
}

esp_err_t bsp_read_qmi(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    return qmi8658_read6(ax, ay, az, gx, gy, gz);
}

esp_err_t bsp_init_wifi_apsta(void)
{
    if (s_wifi_inited) {
        wifi_config_t ap_cfg;
        wifi_fill_open_ap_cfg(&ap_cfg, "Matrix-Wifi");
        return wifi_start_apsta_with_ap_cfg(&ap_cfg, ESP_ERR_WIFI_NOT_STARTED);
    }

    esp_err_t r = nvs_ensure_inited();
    if (r != ESP_OK) return r;

    r = esp_netif_init();
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) return r;

    r = esp_event_loop_create_default();
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) return r;

    if (!s_netif_sta) s_netif_sta = esp_netif_create_default_wifi_sta();
    if (!s_netif_ap) s_netif_ap = esp_netif_create_default_wifi_ap();
    if (!s_netif_sta || !s_netif_ap) return ESP_ERR_NO_MEM;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    r = esp_wifi_init(&cfg);
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) return r;

    wifi_config_t ap_cfg;
    wifi_fill_open_ap_cfg(&ap_cfg, "WS_Matrix");
    r = wifi_start_apsta_with_ap_cfg(&ap_cfg, ESP_ERR_WIFI_NOT_INIT);
    if (r != ESP_OK) return r;

    wifi_config_t sta_cfg = { 0 };
    esp_err_t sr = esp_wifi_get_config(WIFI_IF_STA, &sta_cfg);
    const bool sta_cfg_ok = (sr == ESP_OK) && (sta_cfg.sta.ssid[0] != '\0');
    if (!sta_cfg_ok) {
        wifi_config_t set_cfg = { 0 };
        snprintf((char *)set_cfg.sta.ssid, sizeof(set_cfg.sta.ssid), "%s", "CQ793");
        snprintf((char *)set_cfg.sta.password, sizeof(set_cfg.sta.password), "%s", "123456789");
        r = esp_wifi_set_config(WIFI_IF_STA, &set_cfg);
        if (r != ESP_OK) return r;
    }

    s_sta_next_connect_us = esp_timer_get_time() + 5000000;
    esp_wifi_connect();

    s_wifi_inited = true;
    ESP_LOGI(TAG_WIFI, "wifi apsta inited");
    return ESP_OK;
}

esp_err_t bsp_wifi_stop(void)
{
    if (!s_wifi_inited) return ESP_OK;

    esp_err_t r = esp_wifi_stop();
    if (r != ESP_OK && r != ESP_ERR_WIFI_NOT_STARTED) return r;

    esp_err_t mr = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (mr != ESP_OK && mr != ESP_ERR_WIFI_NOT_INIT) return mr;

    s_sta_next_connect_us = 0;
    return ESP_OK;
}

esp_err_t bsp_wifi_get_status(bool *sta_configured,
                                              bool *sta_connected,
                                              uint32_t *sta_ip,
                                              int *sta_rssi,
                                              bool *ap_on,
                                              int *ap_clients,
                                              uint32_t *ap_ip)
{
    if (!s_wifi_inited) return ESP_ERR_INVALID_STATE;

    wifi_config_t sta_cfg = { 0 };
    esp_err_t sr = esp_wifi_get_config(WIFI_IF_STA, &sta_cfg);
    const bool cfg_ok = (sr == ESP_OK) && (sta_cfg.sta.ssid[0] != '\0');
    if (sta_configured) *sta_configured = cfg_ok;

    wifi_ap_record_t apinfo;
    const esp_err_t ar = esp_wifi_sta_get_ap_info(&apinfo);
    const bool conn = (ar == ESP_OK);
    if (sta_connected) *sta_connected = conn;
    if (sta_rssi) *sta_rssi = conn ? (int)apinfo.rssi : 0;

    if (sta_ip) {
        uint32_t ip = 0;
        if (s_netif_sta) {
            esp_netif_ip_info_t ipi;
            esp_err_t ir = esp_netif_get_ip_info(s_netif_sta, &ipi);
            ip = (ir == ESP_OK) ? ipi.ip.addr : 0;
        }
        *sta_ip = ip;
    }

    if (ap_on) {
        wifi_mode_t m = WIFI_MODE_NULL;
        esp_err_t mr = esp_wifi_get_mode(&m);
        const bool on = (mr == ESP_OK) && (m == WIFI_MODE_AP || m == WIFI_MODE_APSTA);
        *ap_on = on;
    }

    if (ap_clients) {
        wifi_sta_list_t list;
        esp_err_t lr = esp_wifi_ap_get_sta_list(&list);
        *ap_clients = (lr == ESP_OK) ? (int)list.num : 0;
    }

    if (ap_ip) {
        uint32_t ip = 0;
        if (s_netif_ap) {
            esp_netif_ip_info_t ipi;
            esp_err_t ir = esp_netif_get_ip_info(s_netif_ap, &ipi);
            ip = (ir == ESP_OK) ? ipi.ip.addr : 0;
        }
        *ap_ip = ip;
    }

    if (cfg_ok && !conn) {
        const int64_t now_us = esp_timer_get_time();
        const bool can_try = (s_sta_next_connect_us == 0) || (now_us >= s_sta_next_connect_us);
        if (can_try) {
            s_sta_next_connect_us = now_us + 5000000;
            esp_wifi_connect();
        }
    }
    return ESP_OK;
}

esp_err_t bsp_rtc_get_time_date(pcf85063a_datetime_t *time)
{
    if (!time) return ESP_ERR_INVALID_ARG;
    if (!s_rtc_inited) return ESP_ERR_INVALID_STATE;
    return pcf85063a_get_time_date(&s_rtc_dev, time);
}

esp_err_t bsp_rtc_set_time_date(pcf85063a_datetime_t time)
{
    if (!s_rtc_inited) return ESP_ERR_INVALID_STATE;
    return pcf85063a_set_time_date(&s_rtc_dev, time);
}

esp_err_t bsp_rtc_enable_alarm(void)
{
    if (!s_rtc_inited) return ESP_ERR_INVALID_STATE;
    return pcf85063a_enable_alarm(&s_rtc_dev);
}

esp_err_t bsp_rtc_set_alarm(pcf85063a_datetime_t time)
{
    if (!s_rtc_inited) return ESP_ERR_INVALID_STATE;
    return pcf85063a_set_alarm(&s_rtc_dev, time);
}

esp_err_t bsp_rtc_int_register(void (*cb)(void *), void *cb_arg)
{
    esp_err_t r = bsp_init_pcf85063a();
    if (r != ESP_OK) return r;

    s_rtc_int_cb = cb;
    s_rtc_int_cb_arg = cb_arg;

    if (!s_gpio_isr_service_inited) {
        r = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) return r;
        s_gpio_isr_service_inited = true;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_PCF85063A_INT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    r = gpio_config(&io_conf);
    if (r != ESP_OK) return r;

    gpio_isr_handler_remove(BSP_PCF85063A_INT_GPIO);
    return gpio_isr_handler_add(BSP_PCF85063A_INT_GPIO, rtc_int_isr, NULL);
}
