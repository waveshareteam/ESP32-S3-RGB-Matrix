#include "esp_log.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "custom_font_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/display.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "Custom_Font_Display";

/* Declare the custom font */
LV_FONT_DECLARE(lv_font_5x7);

/* Define BOOT button GPIO */
#define BOOT_BUTTON_GPIO 0

static int current_mode = 0;
static lv_style_t style_text;
static bool style_initialized = false;

/* Helper to generate gradient text */
/* dest buffer should be large enough: len(src) * 10 + 1 */
static void get_gradient_text(char *dest, const char *src, uint32_t start_hex, uint32_t end_hex)
{
    int len = strlen(src);
    if (len == 0) {
        dest[0] = '\0';
        return;
    }
    
    int r_s = (start_hex >> 16) & 0xFF;
    int g_s = (start_hex >> 8) & 0xFF;
    int b_s = start_hex & 0xFF;
    
    int r_e = (end_hex >> 16) & 0xFF;
    int g_e = (end_hex >> 8) & 0xFF;
    int b_e = end_hex & 0xFF;
    
    char *p = dest;
    
    for (int i = 0; i < len; i++) {
        /* Linear interpolation */
        /* Use (len > 1 ? len - 1 : 1) to avoid division by zero and reach end color */
        int steps = (len > 1) ? (len - 1) : 1;
        
        int r = r_s + (r_e - r_s) * i / steps;
        int g = g_s + (g_e - g_s) * i / steps;
        int b = b_s + (b_e - b_s) * i / steps;
        
        /* LVGL recolor format: #RRGGBB char# */
        p += sprintf(p, "#%02X%02X%02X %c#", r, g, b, src[i]);
    }
    *p = '\0';
}

/* Update display content based on mode */
static void update_display(void)
{
    /* Clean up current screen */
    lv_obj_clean(lv_scr_act());

    /* Initialize style if needed */
    if (!style_initialized) {
        lv_style_init(&style_text);
        lv_style_set_text_font(&style_text, &lv_font_5x7);
        lv_style_set_text_align(&style_text, LV_TEXT_ALIGN_CENTER);
        lv_style_set_text_letter_space(&style_text, 1);
        style_initialized = true;
    }

    char buf[256];

    if (current_mode == 0 || current_mode == 1) {
        /* Mode 0: P2.5 64x32 */
        /* Mode 1: P3 64x32 */

        /* Line 1: Waveshare (Cyan -> Blue) */
        get_gradient_text(buf, "Waveshare", 0x00FFFF, 0x0000FF);
        lv_obj_t *label1 = lv_label_create(lv_scr_act());
        lv_label_set_recolor(label1, true);
        lv_label_set_text(label1, buf);
        lv_obj_add_style(label1, &style_text, 0);
        lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 0);

        /* Line 2: Electronics (Magenta -> Red) */
        get_gradient_text(buf, "Electronics", 0xFF00FF, 0xFF0000);
        lv_obj_t *label2 = lv_label_create(lv_scr_act());
        lv_label_set_recolor(label2, true);
        lv_label_set_text(label2, buf);
        lv_obj_add_style(label2, &style_text, 0);
        lv_obj_set_style_text_letter_space(label2, 0, 0); /* Tight spacing */
        lv_obj_align(label2, LV_ALIGN_TOP_MID, 0, 8);

        /* Line 3: RGB MATRIX (Yellow -> Orange) */
        get_gradient_text(buf, "RGB MATRIX", 0xFFFF00, 0xFF4500); // FF4500 is OrangeRed
        lv_obj_t *label3 = lv_label_create(lv_scr_act());
        lv_label_set_recolor(label3, true);
        lv_label_set_text(label3, buf);
        lv_obj_add_style(label3, &style_text, 0);
        lv_obj_align(label3, LV_ALIGN_TOP_MID, 0, 48); /* Line 7 */

        /* Line 4: Model Name (Green -> Lime) */
        if (current_mode == 0) {
            get_gradient_text(buf, "P2.5 64x32", 0x00FF00, 0x008000);
        } else {
            get_gradient_text(buf, "P3 64x32", 0x00FF00, 0x008000);
        }
        lv_obj_t *label4 = lv_label_create(lv_scr_act());
        lv_label_set_recolor(label4, true);
        lv_label_set_text(label4, buf);
        lv_obj_add_style(label4, &style_text, 0);
        lv_obj_align(label4, LV_ALIGN_TOP_MID, 0, 56); /* Line 8 */

    } else if (current_mode == 2) {
        /* Mode 2: P2 64x64 (8 lines repeated) */
        const char *lines[] = {
            "Waveshare", "Electronics", "RGB MATRIX", "P2 64x64",
            "Waveshare", "Electronics", "RGB MATRIX", "P2 64x64"
        };
        /* Define gradients for each line */
        uint32_t start_colors[] = {
            0x00FFFF, 0xFF00FF, 0xFFFF00, 0x00FF00,
            0x00FFFF, 0xFF00FF, 0xFFFF00, 0x00FF00
        };
        uint32_t end_colors[] = {
            0x0000FF, 0xFF0000, 0xFF4500, 0x008000,
            0x0000FF, 0xFF0000, 0xFF4500, 0x008000
        };

        for (int i = 0; i < 8; i++) {
            get_gradient_text(buf, lines[i], start_colors[i], end_colors[i]);
            
            lv_obj_t *lbl = lv_label_create(lv_scr_act());
            lv_label_set_recolor(lbl, true);
            lv_label_set_text(lbl, buf);
            lv_obj_add_style(lbl, &style_text, 0);
            
            if (i == 1 || i == 5) { /* Electronics lines */
                lv_obj_set_style_text_letter_space(lbl, 0, 0);
            }

            lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, i * 8);
        }
    }
}

/* LVGL timer task */
static void lvgl_task(void *arg)
{
    /* Configure BOOT button */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    int last_btn_state = 1;

    while (1) {
        lv_timer_handler();

        /* Poll button */
        int btn_state = gpio_get_level(BOOT_BUTTON_GPIO);
        if (last_btn_state == 1 && btn_state == 0) { /* Falling edge */
            vTaskDelay(pdMS_TO_TICKS(50)); /* Debounce */
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                current_mode++;
                if (current_mode > 2) current_mode = 0;
                
                ESP_LOGI(TAG, "Switching to Mode %d", current_mode);
                update_display();
            }
        }
        last_btn_state = btn_state;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void custom_font_display_init(void)
{
    ESP_LOGI(TAG, "Initializing Custom Font Display example");

    /* Initialize display (including LVGL) */
    esp_err_t r = bsp_init_display();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "LVGL init failed: %s", esp_err_to_name(r));
        return;
    }

    /* Set background to black */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    /* Initial display update */
    update_display();

    /* Create LVGL task */
    xTaskCreate(lvgl_task, "lvgl", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "Custom Font Display example initialized");
}
