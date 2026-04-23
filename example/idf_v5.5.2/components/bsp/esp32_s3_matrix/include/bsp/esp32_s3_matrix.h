#pragma once

#include "sdkconfig.h"
#include "config.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_codec_dev.h"
#include "esp_vfs_fat.h"
#include "iot_button.h"
#include "sdmmc_cmd.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 *
 *  SD card interface
 *
**************************************************************************************************/
#if BSP_CAPS_SDCARD

#define CONFIG_BSP_SPIFFS_MOUNT_POINT "/spiffs"
#define CONFIG_BSP_SPIFFS_PARTITION_LABEL NULL
#define CONFIG_BSP_SPIFFS_MAX_FILES 5
#define CONFIG_BSP_SD_MOUNT_POINT "/sdcard"

#define BSP_SPIFFS_MOUNT_POINT      CONFIG_BSP_SPIFFS_MOUNT_POINT
#define BSP_SD_MOUNT_POINT          CONFIG_BSP_SD_MOUNT_POINT
#define BSP_SDSPI_HOST              (SPI2_HOST)
/**
 * @brief Mount SPIFFS partition
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_spiffs_mount(void);

/**
 * @brief Unmount SPIFFS partition
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_spiffs_unmount(void);

/**
 * @brief SD card mount configuration
 */
typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_host_t *host;
    union {
        const sdmmc_slot_config_t *sdmmc;
        const sdspi_device_config_t *sdspi;
    } slot;
} bsp_sdcard_cfg_t;

/**
 * @brief Mount SD card using SDMMC host
 *
 * @param[in,out] cfg Mount and bus configuration
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg);

/**
 * @brief Mount SD card using SDSPI host
 *
 * @param[in,out] cfg Mount and bus configuration
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);

/**
 * @brief Mount SD card using default board settings
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_sdcard_mount(void);

/**
 * @brief Unmount SD card and release related resources
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_sdcard_unmount(void);

/**
 * @brief Fill SDMMC host config with board defaults
 *
 * @param[in]  slot   SDMMC slot index
 * @param[out] config Host config output
 */
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Fill SDSPI host config with board defaults
 *
 * @param[in]  slot   SPI host index
 * @param[out] config Host config output
 */
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Fill SDMMC slot config with board defaults
 *
 * @param[in]  slot   SDMMC slot index
 * @param[out] config Slot config output
 */
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config);

/**
 * @brief Fill SDSPI device config with board defaults
 *
 * @param[in]  spi_host SPI host index
 * @param[out] config   Device config output
 */
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config);

#endif

/**************************************************************************************************
 *
 * I2S audio interface
 *
**************************************************************************************************/
#if BSP_CAPS_AUDIO

#define BSP_I2S_PORT          I2S_NUM_0
#define BSP_AUDIO_PA_REVERTED (false)
/**
 * @brief Init audio
 *
 * @note There is no deinit audio function. Users can free audio resources by calling i2s_del_channel()
 * @warning The type of i2s_config param is depending on IDF version.
 * @param[in]  i2s_config I2S configuration. Pass NULL to use default values (Mono, duplex, 16bit, 22050 Hz)
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_NOT_SUPPORTED The communication mode is not supported on the current chip
 *      - ESP_ERR_INVALID_ARG   NULL pointer or invalid configuration
 *      - ESP_ERR_NOT_FOUND     No available I2S channel found
 *      - ESP_ERR_NO_MEM        No memory for storing the channel information
 *      - ESP_ERR_INVALID_STATE This channel has not initialized or already started
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);

/**
 * @brief Get current codec data interface
 *
 * @return Pointer to audio codec data interface or NULL when not initialized
 */
const audio_codec_data_if_t *bsp_audio_get_codec_itf(void);

/**
 * @brief Initialize speaker codec device
 *
 * @return Pointer to codec device handle or NULL when error occurred
 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);

/**
 * @brief Initialize microphone codec device
 *
 * @return Pointer to codec device handle or NULL when error occurred
 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

/**
 * @brief Get mounted SD card handle
 *
 * @return Pointer to sdmmc_card_t or NULL when not mounted
 */
sdmmc_card_t *bsp_sdcard_get_handle(void);

#endif

/**************************************************************************************************
 *
 * I2C interface
 *
**************************************************************************************************/
#if BSP_CAPS_I2C

#define BSP_I2C_NUM             0
#define BSP_I2C_FREQ_HZ         400000
#define BSP_I2C_TIMEOUT_MS      100

/**
 * @brief Initialize I2C master bus
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinitialize I2C master bus
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Get I2C master bus handle
 *
 * @note This function ensures I2C is initialized before returning.
 * @return I2C master bus handle
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

#endif

/**************************************************************************************************
 *
 *  Button interface
 *
**************************************************************************************************/
#if BSP_CAPS_BUTTON
/**
 * @brief Create all board buttons
 *
 * @param[out] btn_array      Output button array
 * @param[out] btn_cnt        Number of created buttons, can be NULL
 * @param[in]  btn_array_size Size of btn_array
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG when arguments are invalid
 *      - ESP_FAIL when lower level creation fails
 */
esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size);

#endif
/**************************************************************************************************
 *
 *  Display interface
 *
**************************************************************************************************/
#if BSP_CAPS_DISPLAY
/**
 * @brief Set HUB75 global brightness percentage
 *
 * @param[in] brightness_percent 0~100
 * @return ESP_OK on success
 */

esp_err_t bsp_display_brightness_set(int brightness_percent);
#endif

/**************************************************************************************************
 *
 *  WiFi interface
 *
**************************************************************************************************/
#if BSP_CAPS_WIFI
/**
 * @brief Initialize WiFi station and access point
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_init_wifi_apsta(const char *sta_ssid, const char *sta_pass);
/**
 * @brief Stop WiFi station and access point
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL or other esp_err_t on failure
 */
esp_err_t bsp_wifi_stop(void);
/**
 * @brief Get WiFi station and access point status
 *
 * @param[out] sta_configured Whether station station is configured
 * @param[out] sta_connected Whether station is connected to an AP
 * @param[out] sta_ip Station IP address
 * @param[out] sta_rssi Station RSSI
 * @param[out] ap_on Whether access point is enabled
 * @param[out] ap_clients Number of clients connected to access point
 * @param[out] ap_ip Access point IP address
 * @return ESP_OK on success
 */
esp_err_t bsp_wifi_get_status(bool *sta_configured,
                              bool *sta_connected,
                              uint32_t *sta_ip,
                              int *sta_rssi,
                              bool *ap_on,
                              int *ap_clients,
                              uint32_t *ap_ip);

#endif
         
#ifdef __cplusplus
}
#endif
