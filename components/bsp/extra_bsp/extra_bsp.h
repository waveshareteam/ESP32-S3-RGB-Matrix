#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "pcf85063a.h"
#include <stdbool.h>
#include <stdint.h>

#define BSP_CAPS_IMU            1
#define BSP_CAPS_RTC            1
#define BSP_CAPS_TEMP_HUM       1
#define BSP_CAPS_WIFI           1

#define BSP_SHTC3_ADDR 0x70
#define BSP_SHTC3_CMD_WAKEUP 0x3517
#define BSP_SHTC3_CMD_SLEEP 0xB098
#define BSP_SHTC3_CMD_MEASURE_NPM 0x7866

#define BSP_QMI8658_ADDR_L 0x6A
#define BSP_QMI8658_ADDR_H 0x6B

#define BSP_PCF85063A_ADDR ((uint8_t)0x51)
#define BSP_PCF85063A_INT_GPIO GPIO_NUM_10

#define BSP_I2C_FREQ_HZ 400000
#define BSP_I2C_TIMEOUT_MS 100


#define BSP_DISPLAY_WIDTH  (CONFIG_HUB75_PANEL_WIDTH * CONFIG_HUB75_LAYOUT_COLS)
#define BSP_DISPLAY_HEIGHT (CONFIG_HUB75_PANEL_HEIGHT * CONFIG_HUB75_LAYOUT_ROWS)

#define BSP_DISPLAY_RGB565(r, g, b) \
    (uint16_t)(((((uint16_t)(r)) & 0xF8U) << 8) | ((((uint16_t)(g)) & 0xFCU) << 3) | ((((uint16_t)(b)) & 0xF8U) >> 3))
    
#define BSP_DISPLAY_SET_PIXEL(x, y, rgb565) bsp_display_set_pixel((uint16_t)(x), (uint16_t)(y), (uint16_t)(rgb565))
#define BSP_DISPLAY_CLEAR_PIXEL(x, y)      bsp_display_set_pixel((uint16_t)(x), (uint16_t)(y), 0)

esp_err_t bsp_init_shtc3(void);
esp_err_t bsp_read_shtc3(float *temp_c, float *hum_rh);

esp_err_t bsp_init_qmi8658(void);
esp_err_t bsp_read_qmi(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);

esp_err_t bsp_init_pcf85063a(void);
esp_err_t bsp_rtc_get_time_date(pcf85063a_datetime_t *time);
esp_err_t bsp_rtc_set_time_date(pcf85063a_datetime_t time);
esp_err_t bsp_rtc_enable_alarm(void);
esp_err_t bsp_rtc_set_alarm(pcf85063a_datetime_t time);
esp_err_t bsp_rtc_int_register(void (*cb)(void *), void *cb_arg);

esp_err_t bsp_init_wifi_apsta(void);
esp_err_t bsp_wifi_stop(void);
esp_err_t bsp_wifi_get_status(bool *sta_configured,
                                              bool *sta_connected,
                                              uint32_t *sta_ip,
                                              int *sta_rssi,
                                              bool *ap_on,
                                              int *ap_clients,
                                              uint32_t *ap_ip);




esp_err_t bsp_display_set_pixel(uint16_t x, uint16_t y, uint16_t rgb565);
