/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/esp32_s3_matrix.h"

#include "bsp_err_check.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

#include "sdmmc_cmd.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#include "es7210_adc.h"
#include "es8311_codec.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sdkconfig.h>

static const char *TAG = "esp32_s3_matrix";
static const char *TAG_AUDIO = "AUDIO";

bool hub75_bridge_init(void);
void hub75_bridge_draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *buffer, bool big_endian);
void hub75_bridge_deinit(void);

static uint8_t *s_lvgl_buf1 = NULL;
static uint8_t *s_lvgl_buf2 = NULL;
static esp_timer_handle_t s_lvgl_tick_timer = NULL;
static lv_display_t *s_lvgl_disp = NULL;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (!disp || !area || !px_map) {
        return;
    }

    const uint16_t w = (uint16_t)((area->x2 - area->x1) + 1);
    const uint16_t h = (uint16_t)((area->y2 - area->y1) + 1);
    hub75_bridge_draw((uint16_t)area->x1, (uint16_t)area->y1, w, h, px_map, false);
    lv_display_flush_ready(disp);
}

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(5);
}

esp_err_t bsp_init_display(void)
{
    if (s_lvgl_disp) {
        return ESP_OK;
    }

    lv_init();

    const int disp_w = CONFIG_HUB75_PANEL_WIDTH * CONFIG_HUB75_LAYOUT_COLS;
    const int disp_h = CONFIG_HUB75_PANEL_HEIGHT * CONFIG_HUB75_LAYOUT_ROWS;
    const size_t buf_bytes = (size_t)disp_w * (size_t)disp_h * 2;

    s_lvgl_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_lvgl_buf1) {
        s_lvgl_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!s_lvgl_buf1) {
        s_lvgl_buf1 = (uint8_t *)lv_malloc(buf_bytes);
    }
    if (!s_lvgl_buf1) {
        return ESP_ERR_NO_MEM;
    }

    s_lvgl_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_lvgl_buf2) {
        s_lvgl_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (!hub75_bridge_init()) {
        return ESP_FAIL;
    }

    s_lvgl_disp = lv_display_create(disp_w, disp_h);
    lv_display_set_color_format(s_lvgl_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_lvgl_disp, flush_cb);
    lv_display_set_buffers(s_lvgl_disp, s_lvgl_buf1, s_lvgl_buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_timer_create_args_t args = {
        .callback = &lv_tick_cb,
        .arg = NULL,
        .name = "lv_tick",
    };

    esp_err_t r = esp_timer_create(&args, &s_lvgl_tick_timer);
    if (r != ESP_OK) {
        return r;
    }
    return esp_timer_start_periodic(s_lvgl_tick_timer, 5000);
}

static i2c_master_bus_handle_t i2c_handle = NULL;
static bool i2c_initialized = false;

esp_err_t bsp_i2c_init(void)
{
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
    };

    esp_err_t r = i2c_new_master_bus(&bus_cfg, &i2c_handle);
    if (r != ESP_OK) {
        return r;
    }

    i2c_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    bsp_i2c_init();
    return i2c_handle;
}

sdmmc_card_t *bsp_sdcard = NULL;
static bool spi_sd_initialized = false;

static int bsp_sdmmc_width(void)
{
    return (BSP_SD_D1 != GPIO_NUM_NC && BSP_SD_D2 != GPIO_NUM_NC && BSP_SD_D3 != GPIO_NUM_NC) ? 4 : 1;
}

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);
    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0;
    size_t used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
        return ret_val;
    }
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", (int)total, (int)used);
    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return bsp_sdcard;
}

void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config)
{
    (void)slot;
    assert(config);

    sdmmc_host_t host_config = SDMMC_HOST_DEFAULT();
    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    assert(config);

    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = slot;
    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config)
{
    (void)slot;
    assert(config);

    memset(config, 0, sizeof(sdmmc_slot_config_t));
    config->cd = SDMMC_SLOT_NO_CD;
    config->wp = SDMMC_SLOT_NO_WP;
    config->cmd = BSP_SD_CMD;
    config->clk = BSP_SD_CLK;
    config->d0 = BSP_SD_D0;
    config->width = bsp_sdmmc_width();
    config->d1 = (config->width == 4) ? BSP_SD_D1 : GPIO_NUM_NC;
    config->d2 = (config->width == 4) ? BSP_SD_D2 : GPIO_NUM_NC;
    config->d3 = (config->width == 4) ? BSP_SD_D3 : GPIO_NUM_NC;
    config->flags = 0;
}

void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config)
{
    assert(config);

    memset(config, 0, sizeof(sdspi_device_config_t));
    config->gpio_cs = BSP_SD_SPI_CS;
    config->gpio_cd = SDSPI_SLOT_NO_CD;
    config->gpio_wp = SDSPI_SLOT_NO_WP;
    config->gpio_int = GPIO_NUM_NC;
    config->host_id = spi_host;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    config->gpio_wp_polarity = SDSPI_IO_ACTIVE_LOW;
#endif
}

esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg)
{
    sdmmc_host_t sdhost = {0};
    sdmmc_slot_config_t sdslot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    assert(cfg);

#ifdef BSP_SD_POWER
    {
        gpio_config_t power_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << BSP_SD_POWER,
        };
        ESP_ERROR_CHECK(gpio_config(&power_gpio_config));
        ESP_ERROR_CHECK(gpio_set_level(BSP_SD_POWER, 0));
    }
#endif

    const int width = bsp_sdmmc_width();
    if ((width == 4) &&
            ((BSP_SD_D1 == GPIO_NUM_NC) || (BSP_SD_D2 == GPIO_NUM_NC) || (BSP_SD_D3 == GPIO_NUM_NC))) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!cfg->mount) {
        cfg->mount = &mount_config;
    }
    if (!cfg->host) {
        bsp_sdcard_get_sdmmc_host(SDMMC_HOST_SLOT_0, &sdhost);
        cfg->host = &sdhost;
    }
    if (!cfg->slot.sdmmc) {
        bsp_sdcard_sdmmc_get_slot(SDMMC_HOST_SLOT_0, &sdslot);
        cfg->slot.sdmmc = &sdslot;
    }

#if !CONFIG_FATFS_LONG_FILENAMES
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    return esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdmmc, cfg->mount, &bsp_sdcard);
}

esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg)
{
    sdmmc_host_t sdhost = {0};
    sdspi_device_config_t sdslot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    assert(cfg);

#ifdef BSP_SD_POWER
    {
        gpio_config_t power_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << BSP_SD_POWER,
        };
        ESP_ERROR_CHECK(gpio_config(&power_gpio_config));
        ESP_ERROR_CHECK(gpio_set_level(BSP_SD_POWER, 0));
    }
#endif

    if ((BSP_SD_SPI_CLK == GPIO_NUM_NC) || (BSP_SD_SPI_MOSI == GPIO_NUM_NC) || (BSP_SD_SPI_MISO == GPIO_NUM_NC)) {
        return ESP_ERR_INVALID_ARG;
    }

    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_SD_SPI_CLK,
        .mosi_io_num = BSP_SD_SPI_MOSI,
        .miso_io_num = BSP_SD_SPI_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4000,
    };

    if (!spi_sd_initialized) {
        ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_SDSPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");
        spi_sd_initialized = true;
    }

    if (!cfg->mount) {
        cfg->mount = &mount_config;
    }
    if (!cfg->host) {
        bsp_sdcard_get_sdspi_host(BSP_SDSPI_HOST, &sdhost);
        cfg->host = &sdhost;
    }
    if (!cfg->slot.sdspi) {
        bsp_sdcard_sdspi_get_slot(BSP_SDSPI_HOST, &sdslot);
        cfg->slot.sdspi = &sdslot;
    }

#if !CONFIG_FATFS_LONG_FILENAMES
    ESP_LOGW(TAG, "Warning: Long filenames on SD card are disabled in menuconfig!");
#endif

    return esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdspi, cfg->mount, &bsp_sdcard);
}

esp_err_t bsp_sdcard_mount(void)
{
    if (bsp_sdcard) {
        return ESP_OK;
    }

    gpio_pullup_en(BSP_SD_CMD);
    gpio_pullup_en(BSP_SD_D0);

    const int width = bsp_sdmmc_width();
    if (width == 4) {
        if (BSP_SD_D1 != GPIO_NUM_NC) {
            gpio_pullup_en(BSP_SD_D1);
        }
        if (BSP_SD_D2 != GPIO_NUM_NC) {
            gpio_pullup_en(BSP_SD_D2);
        }
        if (BSP_SD_D3 != GPIO_NUM_NC) {
            gpio_pullup_en(BSP_SD_D3);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    bsp_sdcard_cfg_t cfg = {0};
    return bsp_sdcard_sdmmc_mount(&cfg);
}

esp_err_t bsp_sdcard_unmount(void)
{
    esp_err_t ret = ESP_OK;

    ret |= esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
    bsp_sdcard = NULL;

    if (spi_sd_initialized) {
        ret |= spi_bus_free(BSP_SDSPI_HOST);
        spi_sd_initialized = false;
    }

#ifdef BSP_SD_POWER
    gpio_reset_pin(BSP_SD_POWER);
#endif

    return ret;
}


static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
static const audio_codec_data_if_t *s_i2s_data_if = NULL;

#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = BSP_I2S_MCLK,  \
        .bclk = BSP_I2S_SCLK,  \
        .ws = BSP_I2S_LCLK,    \
        .dout = BSP_I2S_DOUT,  \
        .din = BSP_I2S_DSIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

const audio_codec_data_if_t *bsp_audio_get_codec_itf(void)
{
    return s_i2s_data_if;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (i2s_tx_chan && i2s_rx_chan) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));

    const i2s_std_config_t std_cfg_default = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = BSP_I2S_GPIO_CFG,
    };

    const i2s_std_config_t *p_cfg = i2s_config ? i2s_config : &std_cfg_default;

    if (i2s_tx_chan) {
        ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, p_cfg), TAG_AUDIO, "I2S TX init failed");
        ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_tx_chan), TAG_AUDIO, "I2S TX enable failed");
    }
    if (i2s_rx_chan) {
        ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_chan, p_cfg), TAG_AUDIO, "I2S RX init failed");
        ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_rx_chan), TAG_AUDIO, "I2S RX enable failed");
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = BSP_I2S_PORT,
        .rx_handle = i2s_rx_chan,
        .tx_handle = i2s_tx_chan,
    };
    s_i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    BSP_NULL_CHECK(s_i2s_data_if, ESP_ERR_NO_MEM);
    return ESP_OK;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    const audio_codec_data_if_t *i2s_data_if = bsp_audio_get_codec_itf();
    if (!i2s_data_if) {
        if (bsp_i2c_init() != ESP_OK) {
            return NULL;
        }
        if (bsp_audio_init(NULL) != ESP_OK) {
            return NULL;
        }
        i2s_data_if = bsp_audio_get_codec_itf();
    }
    if (!i2s_data_if) {
        return NULL;
    }

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (!gpio_if) {
        return NULL;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!i2c_ctrl_if) {
        return NULL;
    }

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = BSP_AUDIO_PA_REVERTED,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    if (!es8311_dev) {
        return NULL;
    }

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_dev_cfg);
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    const audio_codec_data_if_t *i2s_data_if = bsp_audio_get_codec_itf();
    if (!i2s_data_if) {
        if (bsp_i2c_init() != ESP_OK) {
            return NULL;
        }
        if (bsp_audio_init(NULL) != ESP_OK) {
            return NULL;
        }
        i2s_data_if = bsp_audio_get_codec_itf();
    }
    if (!i2s_data_if) {
        return NULL;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES7210_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!i2c_ctrl_if) {
        return NULL;
    }

    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = i2c_ctrl_if,
    };
    const audio_codec_if_t *es7210_dev = es7210_codec_new(&es7210_cfg);
    if (!es7210_dev) {
        return NULL;
    }

    esp_codec_dev_cfg_t codec_es7210_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7210_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_es7210_dev_cfg);
}

esp_err_t bsp_init(void)
{
    esp_err_t r = bsp_init_display();
    if (r != ESP_OK) {
        return r;
    }
    r = bsp_i2c_init();
    if (r != ESP_OK) {
        return r;
    }
    r = bsp_sdcard_mount();
    if (r != ESP_OK) {
        return r;
    }
    return ESP_OK;
}

