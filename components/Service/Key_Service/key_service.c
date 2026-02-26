#include "key_service.h"

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static TaskHandle_t s_task = NULL;
static key_service_config_t s_cfg;
static key_service_cb_t s_cb = NULL;
static void *s_cb_user = NULL;

static void key_service_emit(key_service_evt_t evt)
{
    if (!s_cb) return;
    s_cb(evt, s_cb_user);
}

static void key_service_gpio_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << s_cfg.gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

static bool key_service_is_pressed(int level)
{
    return level == s_cfg.active_level;
}

static void key_service_task(void *arg)
{
    (void)arg;

    const uint32_t poll_ms = (s_cfg.poll_interval_ms == 0) ? 10U : s_cfg.poll_interval_ms;
    const uint32_t db_ms = (s_cfg.debounce_ms == 0) ? 30U : s_cfg.debounce_ms;
    const uint32_t dbl_ms = (s_cfg.double_click_ms == 0) ? 350U : s_cfg.double_click_ms;
    const uint32_t long_ms = (s_cfg.long_press_ms == 0) ? 800U : s_cfg.long_press_ms;
    const uint32_t rep_ms = (s_cfg.long_repeat_ms == 0) ? 1000U : s_cfg.long_repeat_ms;

    int last = gpio_get_level(s_cfg.gpio_num);
    int stable = last;
    uint32_t stable_ms = 0;

    int64_t press_us = -1;
    int64_t next_rep_us = 0;
    bool long_active = false;

    bool click_pending = false;
    int64_t click_us = 0;

    while (true) {
        const int64_t now_us = esp_timer_get_time();

        if (click_pending) {
            if ((now_us - click_us) >= (int64_t)dbl_ms * 1000) {
                click_pending = false;
                key_service_emit(KEY_SERVICE_EVT_CLICK);
            }
        }

        const int cur = gpio_get_level(s_cfg.gpio_num);
        stable_ms = (cur == last) ? (stable_ms + poll_ms) : 0U;
        last = cur;

        if (stable_ms < db_ms) {
            vTaskDelay(pdMS_TO_TICKS(poll_ms));
            continue;
        }

        if (stable == cur) {
            const bool pressed = key_service_is_pressed(stable);
            if (pressed) {
                if (press_us >= 0 && now_us >= next_rep_us) {
                    while (now_us >= next_rep_us) {
                        if ((now_us - press_us) >= (int64_t)long_ms * 1000) {
                            long_active = true;
                            click_pending = false;
                            key_service_emit(KEY_SERVICE_EVT_LONG_REPEAT);
                            next_rep_us += (int64_t)rep_ms * 1000;
                            continue;
                        }
                        next_rep_us += (int64_t)rep_ms * 1000;
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(poll_ms));
            continue;
        }

        stable = cur;
        const bool pressed = key_service_is_pressed(cur);
        if (pressed) {
            press_us = now_us;
            next_rep_us = now_us + (int64_t)rep_ms * 1000;
            long_active = false;
            vTaskDelay(pdMS_TO_TICKS(poll_ms));
            continue;
        }

        const int64_t dur_us = (press_us >= 0) ? (now_us - press_us) : 0;
        press_us = -1;

        if (long_active) {
            vTaskDelay(pdMS_TO_TICKS(poll_ms));
            continue;
        }

        const bool long_press = dur_us >= (int64_t)long_ms * 1000;
        if (long_press) {
            vTaskDelay(pdMS_TO_TICKS(poll_ms));
            continue;
        }

        if (click_pending) {
            const int64_t dt = now_us - click_us;
            if (dt < (int64_t)dbl_ms * 1000) {
                click_pending = false;
                key_service_emit(KEY_SERVICE_EVT_DOUBLE_CLICK);
                vTaskDelay(pdMS_TO_TICKS(poll_ms));
                continue;
            }
        }

        click_pending = true;
        click_us = now_us;
        vTaskDelay(pdMS_TO_TICKS(poll_ms));
    }
}

esp_err_t key_service_start(const key_service_config_t *cfg, key_service_cb_t cb, void *user)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (s_task) return ESP_OK;

    s_cfg = *cfg;
    s_cb = cb;
    s_cb_user = user;

    key_service_gpio_init();
    BaseType_t ok = xTaskCreate(key_service_task, "key", 2048, NULL, tskIDLE_PRIORITY + 3, &s_task);
    if (ok != pdPASS) {
        s_task = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

void key_service_stop(void)
{
    if (!s_task) return;

    TaskHandle_t t = s_task;
    s_task = NULL;
    vTaskDelete(t);

    s_cb = NULL;
    s_cb_user = NULL;
}

