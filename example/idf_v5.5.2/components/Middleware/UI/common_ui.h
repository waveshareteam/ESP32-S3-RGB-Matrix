#pragma once
#include "esp_err.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  lv_obj_t *line1_label;
  lv_obj_t *line2_label;
  lv_obj_t *line3_label;
  lv_obj_t *line4_label;

  char line1_text[96];
  char line2_text[96];
  char line3_text[96];
  char line4_text[96];
} common_ui;

typedef common_ui example_ui_t;

common_ui *common_ui_get(void);

/*
 * @brief Format fixed-point value (x10) to one decimal text.
 * @param dest Destination buffer to store the formatted text.
 * @param size Size of the destination buffer.
 * @param val_x10 Fixed-point value to format.
 * @return void
 */
void middle_fmt_fixed1(char *dest, size_t size, int32_t val_x10);

/*
 * @brief Install a periodic LVGL timer callback.
 * @param period_ms Period in milliseconds.
 * @param cb Callback function.
 * @return void
 */
void ui_create_timer(uint32_t period_ms, lv_timer_cb_t cb);

/**
 * @brief Initialize common example screen widgets and layout.
 */
void common_ui_init(void);

/**
 * @brief Set label text with gradient color.
 * @param label LVGL label object.
 * @param text Text to set.
 * @param start_color Start color of the gradient.
 * @param end_color End color of the gradient.
 * @return void
 */
void set_label_gradient_text(lv_obj_t *label, const char *text,
                             uint32_t start_color, uint32_t end_color);

#ifdef __cplusplus
}
#endif
