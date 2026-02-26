#pragma once

#include "esp_err.h"

#include "driver/gpio.h"

#include <stdint.h>

typedef enum {
    KEY_SERVICE_EVT_CLICK = 0,
    KEY_SERVICE_EVT_DOUBLE_CLICK,
    KEY_SERVICE_EVT_LONG_REPEAT,
} key_service_evt_t;

typedef struct {
    gpio_num_t gpio_num;
    int active_level;
    uint32_t poll_interval_ms;
    uint32_t debounce_ms;
    uint32_t double_click_ms;
    uint32_t long_press_ms;
    uint32_t long_repeat_ms;
} key_service_config_t;

typedef void (*key_service_cb_t)(key_service_evt_t evt, void *user);

esp_err_t key_service_start(const key_service_config_t *cfg, key_service_cb_t cb, void *user);
void key_service_stop(void);

