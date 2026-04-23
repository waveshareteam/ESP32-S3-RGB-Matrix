#pragma once
#include "esp_err.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp32_s3_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;
    size_t buffer_size;
    bool double_buffer;
    struct {
        unsigned int buff_dma: 1;
        unsigned int buff_spiram: 1;
    } flags;
} bsp_display_cfg;

#if BSP_CAPS_DISPLAY

typedef enum {
    BSP_DISPLAY_MAP_EXTEND = 0,
    BSP_DISPLAY_MAP_MIRROR = 1,
} bsp_display_map_mode_t;

esp_err_t init_display(void);

lv_display_t *bsp_display_start(void);
esp_err_t bsp_display_stop(void); 
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);

/*
 * @brief start display driver with config
 * @param cfg display config
 * @return lv_display_t* display driver instance
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg *cfg); 
/*
 * @brief set display brightness
 * @param brightness_percent brightness percent
 * @return esp_err_t ESP_OK if success
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);
/*
 * @brief set display map mode
 * @param mode display map mode
 * @return uint16_t display map mode
 */
uint16_t bsp_display_set_map_mode(bsp_display_map_mode_t mode);
/*
 * @brief rotate display
 * @param disp display instance
 * @param rotation rotation
 * @return void
 */
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);

#endif

#ifdef __cplusplus
}
#endif
