#include "bsp/esp32_s3_matrix.h"
#include "bsp/display.h"
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
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "esp_lvgl_port.h"
#include "sdmmc_cmd.h"
#include "nvs_flash.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "es7210_adc.h"
#include "es8311_codec.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sdkconfig.h>

bool hub75_bridge_init(void);
void hub75_bridge_draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *buffer, bool big_endian);
void hub75_bridge_flip(void);
void hub75_bridge_set_brightness(uint8_t brightness);
void hub75_bridge_deinit(void);

static const char *TAG = "esp32_s3_matrix";
/* Peripheral boundary: Display + LVGL port + HUB75 bridge (BSP level)
 * - Responsibilities:
 *     Manage LVGL display lifecycle and frame buffers.
 *     Bridge LVGL flush pipeline to HUB75 panel driver.
 *     Provide thread-safe display lock/unlock helpers.
 *     Expose brightness/rotation/start/stop public BSP display APIs.
 * - Public APIs:
 *     bsp_display_brightness_set(): set panel brightness (0~100% -> 0~255).
 *     bsp_display_lock()/bsp_display_unlock(): guard LVGL operations in multi-task context.
 *     bsp_display_rotate(): rotate target display with lock protection.
 *     bsp_display_start()/bsp_display_start_with_config(): start display pipeline.
 *     init_display(): compatibility wrapper returning esp_err_t.
 *     bsp_display_stop(): stop and release display resources. */
static bsp_display_map_mode_t display_map_mode =
#if CONFIG_HUB75_LAYOUT_COLS > 1
    BSP_DISPLAY_MAP_EXTEND;
#else
    BSP_DISPLAY_MAP_MIRROR;
#endif

static uint8_t *lvgl_buf1 = NULL;
static uint8_t *lvgl_buf2 = NULL;
static lv_display_t *lvgl_disp = NULL;
static bool lvgl_port_inited = false;
static bool use_double_buffer = false;

static void bsp_display_free_buffers(void)
{
    if (lvgl_buf1) {
        heap_caps_free(lvgl_buf1);
        lvgl_buf1 = NULL;
    }
    if (lvgl_buf2) {
        heap_caps_free(lvgl_buf2);
        lvgl_buf2 = NULL;
    }
}

static inline int bsp_display_logical_width(void)
{
    const bool mirror = (display_map_mode == BSP_DISPLAY_MAP_MIRROR);
    if (mirror) return CONFIG_HUB75_PANEL_WIDTH;
    return CONFIG_HUB75_PANEL_WIDTH * CONFIG_HUB75_LAYOUT_COLS;
}

static inline int bsp_display_logical_height(void)
{
    return CONFIG_HUB75_PANEL_HEIGHT * CONFIG_HUB75_LAYOUT_ROWS;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (!disp || !area || !px_map) {
        return;
    }

    const uint16_t w = (uint16_t)((area->x2 - area->x1) + 1);
    const uint16_t h = (uint16_t)((area->y2 - area->y1) + 1);
    const uint16_t panel_w = (uint16_t)CONFIG_HUB75_PANEL_WIDTH;
    const uint16_t panel_count = (uint16_t)CONFIG_HUB75_LAYOUT_COLS;
    const bool mirror = (display_map_mode == BSP_DISPLAY_MAP_MIRROR);
    if (!mirror || panel_count <= 1) {
        hub75_bridge_draw((uint16_t)area->x1, (uint16_t)area->y1, w, h, px_map, false);
    }
    uint16_t col = 0;
    while (mirror && col < panel_count) {
        const uint16_t x = (uint16_t)((uint16_t)area->x1 + (uint16_t)(col * panel_w));
        hub75_bridge_draw(x, (uint16_t)area->y1, w, h, px_map, false);
        ++col;
    }
#if defined(CONFIG_HUB75_DOUBLE_BUFFER)
    if (use_double_buffer) {
        hub75_bridge_flip();
    }
#endif
    lv_display_flush_ready(disp);
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) brightness_percent = 100;
    if (brightness_percent < 0) brightness_percent = 0;

    const uint32_t b = (uint32_t)((brightness_percent * 255 + 50) / 100);
    hub75_bridge_set_brightness((uint8_t)b);
    return ESP_OK;
}

uint16_t bsp_display_set_map_mode(bsp_display_map_mode_t mode)
{
    const bool mode_ok = (mode == BSP_DISPLAY_MAP_EXTEND || mode == BSP_DISPLAY_MAP_MIRROR);
    if (!mode_ok) return ESP_ERR_INVALID_ARG;
    if (lvgl_disp) return ESP_ERR_INVALID_STATE;

    display_map_mode = mode;
    return display_map_mode;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    if (!lvgl_port_inited) return false;
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    if (!lvgl_port_inited) return;
    lvgl_port_unlock();
}

void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_display_t *target = disp ? disp : lvgl_disp;
    if (!target) return;
    bool locked = bsp_display_lock(1000);
    lv_disp_set_rotation(target, rotation);
    if (locked) {
        bsp_display_unlock();
    }
}

lv_display_t *bsp_display_start(void)
{
    const int disp_w = bsp_display_logical_width();
    const int disp_h = bsp_display_logical_height();
    const bsp_display_cfg cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = (size_t)disp_w * (size_t)disp_h,
        .double_buffer = true,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
        },
    };
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg *cfg)
{
    if (!cfg) return NULL;

    const int disp_w = bsp_display_logical_width();
    const int disp_h = bsp_display_logical_height();

    if (!lvgl_port_inited) {
        esp_err_t r = lvgl_port_init(&cfg->lvgl_port_cfg);
        if (r != ESP_OK) return NULL;
        lvgl_port_inited = true;
    }

    if (lvgl_disp) return lvgl_disp;

    const size_t buffer_pixels = cfg->buffer_size ? cfg->buffer_size : (size_t)disp_w * (size_t)disp_h;
    const size_t buf_bytes = buffer_pixels * 2;

    lvgl_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!lvgl_buf1) {
        lvgl_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!lvgl_buf1) {
        return NULL;
    }

    if (cfg->double_buffer) {
        lvgl_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!lvgl_buf2) {
            lvgl_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (!lvgl_buf2) {
            bsp_display_free_buffers();
            return NULL;
        }
    }

    if (!hub75_bridge_init()) {
        bsp_display_free_buffers();
        return NULL;
    }
    hub75_bridge_set_brightness(CONFIG_HUB75_BRIGHTNESS);

    bool locked = bsp_display_lock(1000);
    lvgl_disp = lv_display_create(disp_w, disp_h);
    if (!lvgl_disp) {
        if (locked) bsp_display_unlock();
        hub75_bridge_deinit();
        bsp_display_free_buffers();
        return NULL;
    }
    use_double_buffer = cfg->double_buffer;
    lv_display_set_color_format(lvgl_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvgl_disp, flush_cb);
    lv_display_set_buffers(lvgl_disp,
                           lvgl_buf1,
                           use_double_buffer ? lvgl_buf2 : NULL,
                           buf_bytes,
                           use_double_buffer ? LV_DISPLAY_RENDER_MODE_FULL : LV_DISPLAY_RENDER_MODE_PARTIAL);
    if (locked) bsp_display_unlock();

    return lvgl_disp;
}

esp_err_t init_display(void)
{
    return bsp_display_start() ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_display_stop(void)
{
    bool locked = bsp_display_lock(1000);
    if (lvgl_disp) {
        lv_display_delete(lvgl_disp);
        lvgl_disp = NULL;
    }
    if (locked) {
        bsp_display_unlock();
    }
    use_double_buffer = false;
    bsp_display_free_buffers();
    hub75_bridge_deinit();
    return ESP_OK;
}

/* Peripheral boundary: I2C master bus (BSP level)
 * - Provides lazy-initialized I2C master bus for on-board peripherals.
 * - Public APIs:
 *     bsp_i2c_init() / bsp_i2c_deinit(): create/delete I2C master bus
 *     bsp_i2c_get_handle(): obtain bus handle for drivers
 * - Consumers:
 *     Audio codecs (ES8311/ES7210), sensors and other I2C devices.
 * - Safe to call multiple times; initialization is guarded by a flag. */
#if BSP_CAPS_I2C
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
#endif

/* Peripheral boundary: SD card (BSP level)
 * - Provides helpers for SDMMC/SDSPI host & slot configuration.
 * - Mount helpers register FATFS to VFS at BSP_SD_MOUNT_POINT:
 *     bsp_sdcard_sdmmc_mount()/bsp_sdcard_sdspi_mount()
 * - Unmount and resource cleanup:
 *     bsp_sdcard_unmount()
 * - Handles pull-ups and optional power pin (BSP_SD_POWER) internally. */
#if BSP_CAPS_SDCARD
static sdmmc_card_t *bsp_sdcard = NULL;
static bool spi_sd_initialized = false;

/* Return SDMMC bus width (4-bit if D1/D2/D3 available, otherwise 1-bit).
 * Used by slot configuration and pull-up setup to decide extra data pins. */
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

/* Fill SDMMC host defaults for given slot index. */
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config)
{
    (void)slot;
    assert(config);

    sdmmc_host_t host_config = SDMMC_HOST_DEFAULT();
    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

/* Fill SDSPI host defaults for given SPI host id. */
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    assert(config);

    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = slot;
    memcpy(config, &host_config, sizeof(sdmmc_host_t));
}

/* Map board SDMMC slot pins and width into sdmmc_slot_config_t. */
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

/* Map board SDSPI slot pins into sdspi_device_config_t. */
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
    /* Mount SD card via SPI (SDSPI): initialize SPI bus if needed, prepare host/slot,
     * then mount FATFS to VFS at BSP_SD_MOUNT_POINT. */
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
    /* High-level helper to mount SD card over SDMMC with pull-ups and default config. */
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
#endif

/* Peripheral boundary: Audio (I2S + codec glue, BSP level)
 * - Provides I2S channel creation/init and exposes an audio data interface for codecs.
 * - Speaker path: ES8311 over I2C (control) + I2S (data) via esp_codec_dev.
 * - Microphone path: ES7210 over I2C (control) + I2S (data) via esp_codec_dev.
 * - Public APIs:
 *     bsp_audio_init(): init I2S channels and enable them
 *     bsp_audio_codec_speaker_init(): create speaker codec device
 *     bsp_audio_codec_microphone_init(): create microphone codec device */
#if BSP_CAPS_AUDIO
static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
static const audio_codec_data_if_t *i2s_data = NULL;

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

const audio_codec_data_if_t *bsp_audio_get_codec(void)
{
    return i2s_data;
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
        ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, p_cfg), TAG, "I2S TX init failed");
        ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_tx_chan), TAG, "I2S TX enable failed");
    }
    if (i2s_rx_chan) {
        ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_chan, p_cfg), TAG, "I2S RX init failed");
        ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_rx_chan), TAG, "I2S RX enable failed");
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = BSP_I2S_PORT,
        .rx_handle = i2s_rx_chan,
        .tx_handle = i2s_tx_chan,
    };
    i2s_data = audio_codec_new_i2s_data(&i2s_cfg);
    BSP_NULL_CHECK(i2s_data, ESP_ERR_NO_MEM);
    return ESP_OK;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    const audio_codec_data_if_t *i2s_data_if = bsp_audio_get_codec();
    if (!i2s_data_if) {
        if (bsp_i2c_init() != ESP_OK) {
            return NULL;
        }
        if (bsp_audio_init(NULL) != ESP_OK) {
            return NULL;
        }
        i2s_data_if = bsp_audio_get_codec();
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
    const audio_codec_data_if_t *i2s_data_if = bsp_audio_get_codec();
    if (!i2s_data_if) {
        if (bsp_i2c_init() != ESP_OK) {
            return NULL;
        }
        if (bsp_audio_init(NULL) != ESP_OK) {
            return NULL;
        }
        i2s_data_if = bsp_audio_get_codec();
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
#endif

/* Peripheral boundary: WiFi + NVS (BSP level)
 * - Maintains NVS/WiFi initialization state and AP/STA netif handles.
 * - Provides AP+STA startup helper and runtime status query.
 * - Public APIs:
 *     bsp_init_wifi_apsta(): initialize and start AP+STA mode.
 *     bsp_wifi_stop(): stop WiFi and clear mode.
 *     bsp_wifi_get_status(): read STA/AP runtime status. */
#if BSP_CAPS_WIFI
static bool nvs_inited = false;
static bool wifi_inited = false;
static esp_netif_t *netif_sta = NULL;
static esp_netif_t *netif_ap = NULL;

static esp_err_t nvs_ensure_inited(void)
{
    if (nvs_inited) return ESP_OK;

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        esp_err_t er = nvs_flash_erase();
        if (er != ESP_OK) return er;
        r = nvs_flash_init();
    }
    nvs_inited = true;
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

static esp_err_t wifi_start_apsta_with_ap_cfg(wifi_config_t *ap_cfg)
{
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    esp_wifi_set_config(WIFI_IF_AP, ap_cfg);

    esp_err_t r = esp_wifi_start();
    if (r == ESP_OK || r == ESP_ERR_WIFI_NOT_STOPPED) return ESP_OK;
    return r;
}

esp_err_t bsp_init_wifi_apsta(const char *sta_ssid, const char *sta_pass)
{
    esp_err_t r = nvs_ensure_inited();
    if (r != ESP_OK) return r;
    esp_netif_init();
    esp_event_loop_create_default();
    if (!netif_sta) netif_sta = esp_netif_create_default_wifi_sta();
    if (!netif_ap) netif_ap = esp_netif_create_default_wifi_ap();
    if (!netif_sta || !netif_ap) return ESP_ERR_NO_MEM;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t ap_cfg;
    wifi_fill_open_ap_cfg(&ap_cfg, "ESP32_S3_MATRIX");
    wifi_start_apsta_with_ap_cfg(&ap_cfg);
    if (sta_ssid && sta_ssid[0] != '\0') {
        wifi_config_t sta_cfg = { 0 };
        snprintf((char *)sta_cfg.sta.ssid, sizeof(sta_cfg.sta.ssid), "%s", sta_ssid);
        if (sta_pass) {
            snprintf((char *)sta_cfg.sta.password, sizeof(sta_cfg.sta.password), "%s", sta_pass);
        }
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
        esp_wifi_connect();
    }
    wifi_inited = true;
    ESP_LOGI(TAG, "wifi apsta inited");
    return ESP_OK;
}

esp_err_t bsp_wifi_stop(void)
{
    if (!wifi_inited) return ESP_OK;
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
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
    if (!wifi_inited) return ESP_ERR_INVALID_STATE;
    if (sta_configured) *sta_configured = false;
    if (sta_connected) *sta_connected = false;
    if (sta_ip) *sta_ip = 0;
    if (sta_rssi) *sta_rssi = 0;
    if (ap_on) *ap_on = false;
    if (ap_clients) *ap_clients = 0;
    if (ap_ip) *ap_ip = 0;

    if (sta_configured) {
        wifi_config_t sta_cfg = { 0 };
        esp_err_t sr = esp_wifi_get_config(WIFI_IF_STA, &sta_cfg);
        *sta_configured = (sr == ESP_OK) && (sta_cfg.sta.ssid[0] != '\0');
    }

    if (sta_connected || sta_rssi) {
        wifi_ap_record_t apinfo;
        esp_err_t ar = esp_wifi_sta_get_ap_info(&apinfo);
        bool conn = (ar == ESP_OK);
        if (sta_connected) *sta_connected = conn;
        if (sta_rssi && conn) *sta_rssi = (int)apinfo.rssi;
    }

    if (sta_ip && netif_sta) {
        esp_netif_ip_info_t ipi;
        esp_err_t ir = esp_netif_get_ip_info(netif_sta, &ipi);
        if (ir == ESP_OK) *sta_ip = ipi.ip.addr;
    }

    bool ap_enabled = false;
    if (ap_on || ap_clients) {
        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_err_t mr = esp_wifi_get_mode(&mode);
        ap_enabled = (mr == ESP_OK) && (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
        if (ap_on) *ap_on = ap_enabled;
    }

    if (ap_clients && ap_enabled) {
        wifi_sta_list_t list;
        esp_err_t lr = esp_wifi_ap_get_sta_list(&list);
        if (lr == ESP_OK) *ap_clients = (int)list.num;
    }

    if (ap_ip && netif_ap) {
        esp_netif_ip_info_t ipi;
        esp_err_t ir = esp_netif_get_ip_info(netif_ap, &ipi);
        if (ir == ESP_OK) *ap_ip = ipi.ip.addr;
    }

    return ESP_OK;
}
#endif

/* Create a GPIO button device array for the board's main button(s).
 * Outputs:
 *   - btn_array[]: filled with created handles
 *   - btn_cnt: number of buttons created (optional) */

static const button_gpio_config_t btn_main_gpio_cfg = {
    .gpio_num = BSP_BUTTON_MAIN_IO,
    .active_level = 0,
};

esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size)
{
    if (!btn_array || btn_array_size < BSP_BUTTON_NUM) return ESP_ERR_INVALID_ARG;

    if (btn_cnt) {
        *btn_cnt = 0;
    }

    const button_config_t btn_cfg = {0};
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_main_gpio_cfg, &btn_array[BSP_BUTTON_MAIN]);

    if (btn_cnt) {
        *btn_cnt = BSP_BUTTON_NUM;
    }
    return ret;
}
