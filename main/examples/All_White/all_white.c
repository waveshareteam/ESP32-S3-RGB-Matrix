#include "all_white.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/display.h"
#include "lvgl.h"

static const char *TAG = "all_white";

static void set_bg_and_refr(lv_obj_t *scr, uint32_t rgb888)
{
    if (!scr) return;
    lv_obj_set_style_bg_color(scr, lv_color_hex(rgb888), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_refr_now(NULL);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void all_white_start(void)
{
    esp_err_t r = bsp_init_display();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "bsp_init_display failed: %d", (int)r);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    set_bg_and_refr(scr, 0xFF0000);
    vTaskDelay(pdMS_TO_TICKS(1000));
    set_bg_and_refr(scr, 0x00FF00);
    vTaskDelay(pdMS_TO_TICKS(1000));
    set_bg_and_refr(scr, 0x0000FF);
    vTaskDelay(pdMS_TO_TICKS(1000));
    set_bg_and_refr(scr, 0xFFFFFF);

    const BaseType_t ok = xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 10, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "create task failed");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
