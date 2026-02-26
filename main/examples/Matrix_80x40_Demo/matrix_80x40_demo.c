#include "matrix_80x40_demo.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/display.h"
#include "extra_bsp.h"
#include "key_service.h"
#include "lvgl.h"   

static const char *TAG = "m80x40";

LV_FONT_DECLARE(lv_font_5x7);

static lv_style_t s_text_style;
static bool s_text_style_inited = false;

static lv_obj_t *s_bg = NULL;
static lv_timer_t *s_breath_timer = NULL;

typedef enum {
    MODE_WHITE = 0,
    MODE_BREATH,
    MODE_COUNT,
} demo_mode_t;

static demo_mode_t s_mode = MODE_WHITE;
static volatile uint32_t s_key_click_cnt = 0;
static uint32_t s_breath_t_ms = 0;

enum {
    BREATH_TICK_MS = 20,
};

static void grid_set_visible(bool en);

enum {
    GRID_CELL_W = 6,
    GRID_CELL_H = 8,
    GRID_MAX_CELLS = 256,
};

static lv_obj_t *s_cells[GRID_MAX_CELLS];
static uint16_t s_cell_cnt = 0;

static void init_text_style(void)
{
    if (s_text_style_inited) return;
    lv_style_init(&s_text_style);
    lv_style_set_text_font(&s_text_style, &lv_font_5x7);
    lv_style_set_text_align(&s_text_style, LV_TEXT_ALIGN_LEFT);
    lv_style_set_text_letter_space(&s_text_style, 1);
    s_text_style_inited = true;
}

static void set_bg_color(lv_color_t c)
{
    if (!s_bg) return;
    lv_obj_set_style_bg_color(s_bg, c, 0);
}

static void breath_timer_cb(lv_timer_t *t)
{
    (void)t;
    const uint32_t period_ms = 4000;
    const uint32_t half_ms = period_ms / 2;

    s_breath_t_ms += (uint32_t)BREATH_TICK_MS;
    s_breath_t_ms %= period_ms;

    const uint32_t phase_ms = s_breath_t_ms;
    const uint32_t up_ms = (phase_ms < half_ms) ? phase_ms : (period_ms - phase_ms);
    uint32_t level = (up_ms * 255U + (half_ms / 2U)) / half_ms;
    level = (level * level + 255U) / 255U;
    if (level > 255U) level = 255U;

    set_bg_color(lv_color_make((uint8_t)level, (uint8_t)level, (uint8_t)level));
}

static void mode_apply(demo_mode_t mode)
{
    if (s_breath_timer) {
        lv_timer_del(s_breath_timer);
        s_breath_timer = NULL;
    }

    grid_set_visible(false);

    switch (mode) {
        case MODE_BREATH:
            s_breath_t_ms = 0;
            s_breath_timer = lv_timer_create(breath_timer_cb, BREATH_TICK_MS, NULL);
            breath_timer_cb(s_breath_timer);
            break;
        case MODE_WHITE:
        default:
            set_bg_color(lv_color_white());
            break;
    }

    lv_refr_now(NULL);
}

static void demo_key_cb(key_service_evt_t evt, void *user)
{
    (void)user;
    if (evt != KEY_SERVICE_EVT_CLICK) return;
    s_key_click_cnt++;
}

static void grid_set_visible(bool en)
{
    uint16_t i = 0;
    while (i < s_cell_cnt) {
        if (s_cells[i]) {
            if (en) lv_obj_clear_flag(s_cells[i], LV_OBJ_FLAG_HIDDEN);
            if (!en) lv_obj_add_flag(s_cells[i], LV_OBJ_FLAG_HIDDEN);
        }
        i++;
    }
}

static void setup_screen(uint16_t w, uint16_t h)
{
    /* 获取当前活动屏幕对象，若获取失败则直接返回 */
    lv_obj_t *scr = lv_screen_active();
    if (!scr) return;

    /* 清空屏幕上的所有对象，并设置屏幕背景为不透明黑色 */
    lv_obj_clean(scr);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    /* 创建背景对象，覆盖整个屏幕，并设置为不透明黑色、无圆角、无边框、无阴影、无内边距、不可滚动 */
    s_bg = lv_obj_create(scr);
    lv_obj_set_pos(s_bg, 0, 0);
    lv_obj_set_size(s_bg, (lv_coord_t)w, (lv_coord_t)h);
    lv_obj_set_style_bg_opa(s_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_bg, lv_color_white(), 0);
    lv_obj_set_style_radius(s_bg, 0, 0);
    lv_obj_set_style_border_width(s_bg, 0, 0);
    lv_obj_set_style_outline_width(s_bg, 0, 0);
    lv_obj_set_style_shadow_width(s_bg, 0, 0);
    lv_obj_set_style_pad_all(s_bg, 0, 0);
    lv_obj_clear_flag(s_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 根据屏幕尺寸计算网格行列数，并创建对应数量的标签对象用于显示字符 */
    {
        const uint16_t cols = (uint16_t)(w / GRID_CELL_W);      /* 列数 */
        const uint16_t rows = (uint16_t)(h / GRID_CELL_H);      /* 行数 */
        const uint32_t want = (uint32_t)cols * (uint32_t)rows;  /* 理论总单元格数 */
        const uint16_t cnt = (uint16_t)((want > GRID_MAX_CELLS) ? GRID_MAX_CELLS : want); /* 实际创建数量，受最大限制 */

        s_cell_cnt = 0;
        uint16_t y = 0;
        while (y < rows) {
            uint16_t x = 0;
            while (x < cols) {
                if (s_cell_cnt >= cnt) break; /* 达到上限则停止创建 */

                /* 创建标签对象，应用文本样式，设置位置和尺寸，初始状态为隐藏 */
                lv_obj_t *label = lv_label_create(scr);
                lv_obj_add_style(label, &s_text_style, 0);
                lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
                lv_obj_set_pos(label, (lv_coord_t)(x * GRID_CELL_W), (lv_coord_t)(y * GRID_CELL_H));
                lv_obj_set_size(label, GRID_CELL_W, GRID_CELL_H);
                lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
                s_cells[s_cell_cnt] = label;
                s_cell_cnt++;

                x++;
            }
            if (s_cell_cnt >= cnt) break;
            y++;
        }
    }

    grid_set_visible(false);
    set_bg_color(lv_color_white());
}

static void lvgl_task(void *arg)
{
    (void)arg;

    uint32_t last_click_cnt = 0;

    while (true) {
        const uint32_t cur_click_cnt = s_key_click_cnt;
        while (last_click_cnt != cur_click_cnt) {
            last_click_cnt++;
            s_mode = (demo_mode_t)(((uint32_t)s_mode + 1U) % (uint32_t)MODE_COUNT);
            mode_apply(s_mode);
        }
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms < 2) wait_ms = 2;
        if (wait_ms > 20) wait_ms = 20;
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}


void matrix_80x40_demo_start(void)
{
    const esp_err_t r = bsp_init_display();
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "bsp_init_display failed: %d", (int)r);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    const uint16_t w = (uint16_t)BSP_DISPLAY_WIDTH;
    const uint16_t h = (uint16_t)BSP_DISPLAY_HEIGHT;
    ESP_LOGI(TAG, "display: %ux%u", (unsigned)w, (unsigned)h);

    init_text_style();
    setup_screen(w, h);

    mode_apply(s_mode);
    {
        const key_service_config_t cfg = {
            .gpio_num = GPIO_NUM_0,
            .active_level = 0,
            .poll_interval_ms = 10,
            .debounce_ms = 30,
            .double_click_ms = 350,
            .long_press_ms = 800,
            .long_repeat_ms = 1000,
        };
        const esp_err_t key_r = key_service_start(&cfg, demo_key_cb, NULL);
        if (key_r != ESP_OK) ESP_LOGE(TAG, "key init failed: %s", esp_err_to_name(key_r));
    }

    const BaseType_t ok = xTaskCreatePinnedToCore(lvgl_task, "lvgl", 4096, NULL, 5, NULL, 1);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "create task failed");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
