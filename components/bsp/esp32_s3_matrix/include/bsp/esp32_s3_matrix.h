#pragma once

#include "sdkconfig.h"

#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"

#include "esp_codec_dev.h"
#include "esp_vfs_fat.h"

#include "sdmmc_cmd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_AUDIO          1
#define BSP_CAPS_AUDIO_SPEAKER  1
#define BSP_CAPS_AUDIO_MIC      1
#define BSP_CAPS_SDCARD         1


/**************************************************************************************************
 *  ESP32-S3-Matrix-BSP pinout
 **************************************************************************************************/

#define BSP_I2C_SCL           (GPIO_NUM_48)
#define BSP_I2C_SDA           (GPIO_NUM_47)

#define BSP_I2S_SCLK          (GPIO_NUM_43)
#define BSP_I2S_MCLK          (GPIO_NUM_12)
#define BSP_I2S_LCLK          (GPIO_NUM_38)
#define BSP_I2S_DOUT          (GPIO_NUM_21)
#define BSP_I2S_DSIN          (GPIO_NUM_39)
#define BSP_POWER_AMP_IO      (GPIO_NUM_11)

#define BSP_SD_D0             (GPIO_NUM_17)
#define BSP_SD_D1             (GPIO_NUM_NC)
#define BSP_SD_D2             (GPIO_NUM_NC)
#define BSP_SD_D3             (GPIO_NUM_NC)
#define BSP_SD_CMD            (GPIO_NUM_44)
#define BSP_SD_CLK            (GPIO_NUM_1)

#define BSP_SD_SPI_MISO       (GPIO_NUM_NC)
#define BSP_SD_SPI_CS         (GPIO_NUM_14)
#define BSP_SD_SPI_MOSI       (GPIO_NUM_NC)
#define BSP_SD_SPI_CLK        (GPIO_NUM_NC)

#define CONFIG_BSP_I2C_NUM 0

#define CONFIG_BSP_SPIFFS_MOUNT_POINT "/spiffs"
#define CONFIG_BSP_SPIFFS_PARTITION_LABEL NULL
#define CONFIG_BSP_SPIFFS_MAX_FILES 5

#define CONFIG_BSP_SD_MOUNT_POINT "/sdcard"

/**************************************************************************************************
 *
 * I2C interface
 *
 **************************************************************************************************/
#define BSP_I2C_NUM     CONFIG_BSP_I2C_NUM

/**************************************************************************************************
 *
 * I2S audio interface
 *
 **************************************************************************************************/
#define BSP_I2S_PORT          I2S_NUM_0
#define BSP_AUDIO_PA_REVERTED (false)

/**************************************************************************************************
 *
 * SD card interface
 *
 **************************************************************************************************/
#define BSP_SPIFFS_MOUNT_POINT      CONFIG_BSP_SPIFFS_MOUNT_POINT
#define BSP_SD_MOUNT_POINT          CONFIG_BSP_SD_MOUNT_POINT
#define BSP_SDSPI_HOST              (SPI2_HOST)

esp_err_t bsp_spiffs_mount(void);
esp_err_t bsp_spiffs_unmount(void);

typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_host_t *host;
    union {
        const sdmmc_slot_config_t *sdmmc;
        const sdspi_device_config_t *sdspi;
    } slot;
} bsp_sdcard_cfg_t;

esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg);
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);
esp_err_t bsp_sdcard_mount(void);
esp_err_t bsp_sdcard_unmount(void);

void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config);
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config);
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config);


/**************************************************************************************************
 *
 * I2S audio interface
 *
 * There are two devices connected to the I2S peripheral:
 *  - Codec ES8311 for output(playback) and input(recording) path
 *
 * For speaker initialization use bsp_audio_codec_speaker_init() which is inside initialize I2S with bsp_audio_init().
 * For microphone initialization use bsp_audio_codec_microphone_init() which is inside initialize I2S with bsp_audio_init().
 * After speaker or microphone initialization, use functions from esp_codec_dev for play/record audio.
 * Example audio play:
 * \code{.c}
 * esp_codec_dev_set_out_vol(spk_codec_dev, DEFAULT_VOLUME);
 * esp_codec_dev_open(spk_codec_dev, &fs);
 * esp_codec_dev_write(spk_codec_dev, wav_bytes, bytes_read_from_spiffs);
 * esp_codec_dev_close(spk_codec_dev);
 * \endcode
 **************************************************************************************************/

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

sdmmc_card_t *bsp_sdcard_get_handle(void);



esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

#ifdef __cplusplus
}
#endif
