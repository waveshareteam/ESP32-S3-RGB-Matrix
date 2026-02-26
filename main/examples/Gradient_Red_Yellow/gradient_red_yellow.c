#include "gradient_red_yellow.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/display.h"
#include "extra_bsp.h"
#include "lvgl.h"

static const char *TAG = "grad_ry";

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void fill_red_to_yellow_gradient(uint16_t *buffer, uint16_t width, uint16_t height)
{
    if (!buffer) return;
    if (width == 0) return;
    if (height == 0) return;

    uint16_t col = 0;
    while (col < width) {
        const uint16_t grad_div = (uint16_t)((width > 1) ? (width - 1) : 1);
        const uint16_t green_comp = (uint16_t)((uint32_t)col * 255U / (uint32_t)grad_div);
        const uint16_t pixel_color = BSP_DISPLAY_RGB565(255U, green_comp, 0U);

        uint16_t row = 0;
        while (row < height) {
            buffer[(uint32_t)row * (uint32_t)width + (uint32_t)col] = pixel_color;
            row++;
        }
        col++;
    }
}

void gradient_red_yellow_start(void)
{
    const esp_err_t err = bsp_init_display();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_init_display failed: %d", (int)err);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);

    const uint16_t screen_width = (uint16_t)BSP_DISPLAY_WIDTH;
    const uint16_t screen_height = (uint16_t)BSP_DISPLAY_HEIGHT;
    const uint32_t pixel_count = (uint32_t)screen_width * (uint32_t)screen_height;
    const uint32_t buffer_bytes = pixel_count * 2U;

    uint16_t *frame_buffer = (uint16_t *)lv_malloc(buffer_bytes);
    if (!frame_buffer) {
        ESP_LOGE(TAG, "alloc failed: %u", (unsigned)buffer_bytes);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    fill_red_to_yellow_gradient(frame_buffer, screen_width, screen_height);

    lv_obj_t *canvas_obj = lv_canvas_create(screen);
    lv_obj_set_size(canvas_obj, screen_width, screen_height);
    lv_obj_align(canvas_obj, LV_ALIGN_CENTER, 0, 0);
    lv_canvas_set_buffer(canvas_obj, frame_buffer, screen_width, screen_height, LV_COLOR_FORMAT_RGB565);
    lv_obj_invalidate(canvas_obj);
    lv_refr_now(NULL);

    const BaseType_t task_created = xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 10, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "create task failed");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
