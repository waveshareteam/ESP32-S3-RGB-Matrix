#include "sd_image.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/display.h"
#include "extra_bsp.h"
#include "sdcard_service.h"
#include "key_service.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "sd_image";

static volatile uint32_t s_key_click_cnt = 0;

enum {
    MAX_BMP_FILES = 64,
};

static char *s_bmp_files[MAX_BMP_FILES];
static uint16_t s_bmp_file_cnt = 0;
static uint16_t s_bmp_file_idx = 0;

void hub75_bridge_draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *buffer, bool big_endian);

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t le32s(const uint8_t *p)
{
    return (int32_t)le32(p);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((((uint16_t)r & 0xF8U) << 8) | (((uint16_t)g & 0xFCU) << 3) | (((uint16_t)b & 0xF8U) >> 3));
}

static void draw_fill(uint16_t color)
{
    const uint16_t w = (uint16_t)BSP_DISPLAY_WIDTH;
    const uint16_t h = (uint16_t)BSP_DISPLAY_HEIGHT;

    uint16_t *line = (uint16_t *)heap_caps_malloc((size_t)w * sizeof(uint16_t), MALLOC_CAP_8BIT);
    if (!line) return;

    uint16_t x = 0;
    while (x < w) {
        line[x] = color;
        x++;
    }

    uint16_t y = 0;
    while (y < h) {
        hub75_bridge_draw(0, y, w, 1, (const uint8_t *)line, false);
        y++;
    }

    heap_caps_free(line);
}

static bool ends_with_bmp(const char *s)
{
    if (!s) return false;
    const size_t n = strlen(s);
    if (n < 4) return false;
    const char *e = &s[n - 4];
    const bool a = (e[0] == '.') && ((e[1] == 'b') || (e[1] == 'B'));
    const bool b = ((e[2] == 'm') || (e[2] == 'M')) && ((e[3] == 'p') || (e[3] == 'P'));
    return a && b;
}

static void key_cb(key_service_evt_t evt, void *user)
{
    (void)user;
    if (evt != KEY_SERVICE_EVT_CLICK) return;
    s_key_click_cnt++;
}

static bool is_dot_dir(const char *name)
{
    if (!name) return true;
    if (name[0] != '.') return false;
    if (name[1] == '\0') return true;
    return (name[1] == '.') && (name[2] == '\0');
}

static bool find_first_bmp_in_dir(const char *dir_path, int depth, char *out_path, size_t out_len)
{
    if (!dir_path || !out_path || out_len == 0) return false;

    DIR *d = opendir(dir_path);
    if (!d) {
        ESP_LOGE(TAG, "opendir failed: %s (errno=%d)", dir_path, errno);
        return false;
    }

    bool found = false;
    uint32_t shown = 0;
    uint32_t total = 0;
    while (true) {
        struct dirent *e = readdir(d);
        if (!e) break;
        total++;

        if (shown < 20) {
            ESP_LOGI(TAG, "ls: %s", e->d_name);
            shown++;
        }

        if (is_dot_dir(e->d_name)) continue;

        char full[256];
        const int m = snprintf(full, sizeof(full), "%s/%s", dir_path, e->d_name);
        if (m <= 0 || (size_t)m >= sizeof(full)) continue;

        if (ends_with_bmp(e->d_name)) {
            const int k = snprintf(out_path, out_len, "%s", full);
            found = (k > 0) && ((size_t)k < out_len);
            break;
        }

        if (depth <= 0) continue;

        DIR *sd = opendir(full);
        if (!sd) continue;
        closedir(sd);

        found = find_first_bmp_in_dir(full, depth - 1, out_path, out_len);
        if (found) break;
    }

    if (!found) {
        ESP_LOGW(TAG, "no bmp in %s (entries=%u)", dir_path, (unsigned)total);
    }

    closedir(d);
    return found;
}

static void bmp_list_reset(void)
{
    uint16_t i = 0;
    while (i < s_bmp_file_cnt) {
        if (s_bmp_files[i]) heap_caps_free(s_bmp_files[i]);
        s_bmp_files[i] = NULL;
        i++;
    }
    s_bmp_file_cnt = 0;
    s_bmp_file_idx = 0;
}

static bool bmp_list_push(const char *path)
{
    if (!path) return false;
    if (s_bmp_file_cnt >= MAX_BMP_FILES) return false;

    const size_t n = strlen(path);
    char *p = (char *)heap_caps_malloc(n + 1, MALLOC_CAP_8BIT);
    if (!p) return false;
    memcpy(p, path, n + 1);

    s_bmp_files[s_bmp_file_cnt] = p;
    s_bmp_file_cnt++;
    return true;
}

static void scan_bmp_in_dir(const char *dir_path, int depth)
{
    if (!dir_path) return;
    if (s_bmp_file_cnt >= MAX_BMP_FILES) return;

    DIR *d = opendir(dir_path);
    if (!d) return;

    while (true) {
        struct dirent *e = readdir(d);
        if (!e) break;
        if (is_dot_dir(e->d_name)) continue;

        char full[256];
        const int m = snprintf(full, sizeof(full), "%s/%s", dir_path, e->d_name);
        if (m <= 0 || (size_t)m >= sizeof(full)) continue;

        if (ends_with_bmp(e->d_name)) {
            bmp_list_push(full);
            if (s_bmp_file_cnt >= MAX_BMP_FILES) break;
            continue;
        }

        if (depth <= 0) continue;

        DIR *sd = opendir(full);
        if (!sd) continue;
        closedir(sd);

        scan_bmp_in_dir(full, depth - 1);
        if (s_bmp_file_cnt >= MAX_BMP_FILES) break;
    }

    closedir(d);
}

static void build_bmp_list(void)
{
    bmp_list_reset();

    const char *root = sdcard_service_root();
    if (!root) return;

    const char *candidates[] = {
        "SD_Image",
        "SDIMAGE",
        "SD_IMG",
    };

    {
        size_t i = 0;
        while (i < (sizeof(candidates) / sizeof(candidates[0]))) {
            char p[128];
            const int n = snprintf(p, sizeof(p), "%s/%s", root, candidates[i]);
            if (n > 0 && (size_t)n < sizeof(p)) {
                scan_bmp_in_dir(p, 2);
                if (s_bmp_file_cnt) return;
            }
            i++;
        }
    }

    {
        DIR *d = opendir(root);
        if (d) {
            while (true) {
                struct dirent *e = readdir(d);
                if (!e) break;
                if (is_dot_dir(e->d_name)) continue;

                char sub[256];
                const int n = snprintf(sub, sizeof(sub), "%s/%s", root, e->d_name);
                if (n <= 0 || (size_t)n >= sizeof(sub)) continue;

                DIR *sd = opendir(sub);
                if (!sd) continue;
                closedir(sd);

                size_t i = 0;
                while (i < (sizeof(candidates) / sizeof(candidates[0]))) {
                    char p[256];
                    const int m = snprintf(p, sizeof(p), "%s/%s", sub, candidates[i]);
                    if (m > 0 && (size_t)m < sizeof(p)) {
                        scan_bmp_in_dir(p, 2);
                        if (s_bmp_file_cnt) {
                            closedir(d);
                            return;
                        }
                    }
                    i++;
                }
            }
            closedir(d);
        }
    }

    scan_bmp_in_dir(root, 0);
}

static bool find_first_bmp(char *out_path, size_t out_len)
{
    if (!out_path || out_len == 0) return false;

    const char *root = sdcard_service_root();
    if (!root) return false;

    const char *candidates[] = {
        "SD_Image",
        "SDIMAGE",
        "SD_IMG",
    };

    {
        size_t i = 0;
        while (i < (sizeof(candidates) / sizeof(candidates[0]))) {
            char p[128];
            const int n = snprintf(p, sizeof(p), "%s/%s", root, candidates[i]);
            if (n > 0 && (size_t)n < sizeof(p)) {
                if (find_first_bmp_in_dir(p, 2, out_path, out_len)) return true;
            }
            i++;
        }
    }

    {
        DIR *d = opendir(root);
        if (d) {
            while (true) {
                struct dirent *e = readdir(d);
                if (!e) break;
                if (is_dot_dir(e->d_name)) continue;

                char sub[256];
                const int n = snprintf(sub, sizeof(sub), "%s/%s", root, e->d_name);
                if (n <= 0 || (size_t)n >= sizeof(sub)) continue;

                DIR *sd = opendir(sub);
                if (!sd) continue;
                closedir(sd);

                size_t i = 0;
                while (i < (sizeof(candidates) / sizeof(candidates[0]))) {
                    char p[256];
                    const int m = snprintf(p, sizeof(p), "%s/%s", sub, candidates[i]);
                    if (m > 0 && (size_t)m < sizeof(p)) {
                        if (find_first_bmp_in_dir(p, 2, out_path, out_len)) {
                            closedir(d);
                            return true;
                        }
                    }
                    i++;
                }
            }
            closedir(d);
        }
    }

    return find_first_bmp_in_dir(root, 0, out_path, out_len);
}

static esp_err_t draw_bmp_24(const char *path, uint32_t data_off, uint32_t row_bytes, int32_t w, int32_t h)
{
    if (!path) return ESP_ERR_INVALID_ARG;
    if (w <= 0 || h == 0) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;

    const bool top_down = (h < 0);
    const uint32_t src_h = (uint32_t)(top_down ? -h : h);
    const uint16_t disp_w = (uint16_t)BSP_DISPLAY_WIDTH;
    const uint16_t disp_h = (uint16_t)BSP_DISPLAY_HEIGHT;

    const uint16_t crop_w = (uint16_t)(((uint32_t)w > (uint32_t)disp_w) ? disp_w : (uint32_t)w);
    const uint16_t crop_h = (uint16_t)((src_h > (uint32_t)disp_h) ? disp_h : src_h);

    const uint16_t dst_x0 = (uint16_t)((disp_w > crop_w) ? ((disp_w - crop_w) / 2U) : 0U);
    const uint16_t dst_y0 = (uint16_t)((disp_h > crop_h) ? ((disp_h - crop_h) / 2U) : 0U);

    const uint32_t src_x0 = ((uint32_t)w > (uint32_t)crop_w) ? (((uint32_t)w - (uint32_t)crop_w) / 2U) : 0U;
    const uint32_t src_y0 = (src_h > (uint32_t)crop_h) ? ((src_h - (uint32_t)crop_h) / 2U) : 0U;

    uint8_t *rgb = (uint8_t *)heap_caps_malloc((size_t)crop_w * 3U, MALLOC_CAP_8BIT);
    uint16_t *line = (uint16_t *)heap_caps_malloc((size_t)crop_w * sizeof(uint16_t), MALLOC_CAP_8BIT);
    if (!rgb || !line) {
        if (rgb) heap_caps_free(rgb);
        if (line) heap_caps_free(line);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    uint16_t y = 0;
    while (y < crop_h) {
        const uint32_t sy = src_y0 + (uint32_t)y;
        const uint32_t row_idx = top_down ? sy : ((src_h - 1U) - sy);
        const uint32_t row_off = data_off + row_idx * row_bytes + src_x0 * 3U;

        if (fseek(f, (long)row_off, SEEK_SET) != 0) break;
        const size_t want = (size_t)crop_w * 3U;
        const size_t got = fread(rgb, 1, want, f);
        if (got != want) break;

        uint16_t x = 0;
        while (x < crop_w) {
            const uint8_t b = rgb[(size_t)x * 3U + 0U];
            const uint8_t g = rgb[(size_t)x * 3U + 1U];
            const uint8_t r = rgb[(size_t)x * 3U + 2U];
            line[x] = rgb565(r, g, b);
            x++;
        }

        hub75_bridge_draw(dst_x0, (uint16_t)(dst_y0 + y), crop_w, 1, (const uint8_t *)line, false);
        y++;
    }

    heap_caps_free(rgb);
    heap_caps_free(line);
    fclose(f);
    return (y == crop_h) ? ESP_OK : ESP_FAIL;
}

static esp_err_t draw_bmp(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;

    uint8_t hdr[54];
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;

    const size_t got = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    if (got != sizeof(hdr)) return ESP_FAIL;

    if (hdr[0] != 'B' || hdr[1] != 'M') return ESP_ERR_NOT_SUPPORTED;

    const uint32_t off_bits = le32(&hdr[10]);
    const uint32_t dib_size = le32(&hdr[14]);
    if (dib_size < 40) return ESP_ERR_NOT_SUPPORTED;

    const int32_t w = le32s(&hdr[18]);
    const int32_t h = le32s(&hdr[22]);
    const uint16_t planes = le16(&hdr[26]);
    const uint16_t bpp = le16(&hdr[28]);
    const uint32_t comp = le32(&hdr[30]);

    if (planes != 1) return ESP_ERR_NOT_SUPPORTED;
    if (bpp != 24) return ESP_ERR_NOT_SUPPORTED;
    if (comp != 0) return ESP_ERR_NOT_SUPPORTED;
    if (w <= 0 || h == 0) return ESP_ERR_NOT_SUPPORTED;

    const uint32_t row_bytes = (uint32_t)((((uint32_t)w * 3U) + 3U) & ~3U);
    return draw_bmp_24(path, off_bits, row_bytes, w, h);
}

void sd_image_start(void)
{
    const esp_err_t dr = bsp_init_display();
    if (dr != ESP_OK) {
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    draw_fill(rgb565(0, 0, 0));

    const esp_err_t sr = sdcard_service_init();
    if (sr != ESP_OK) {
        ESP_LOGE(TAG, "sd init failed: %s", esp_err_to_name(sr));
        draw_fill(rgb565(255, 0, 0));
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

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
        const esp_err_t key_r = key_service_start(&cfg, key_cb, NULL);
        if (key_r != ESP_OK) ESP_LOGE(TAG, "key init failed: %s", esp_err_to_name(key_r));
    }

    build_bmp_list();
    if (s_bmp_file_cnt == 0) {
        char path[192];
        const bool ok = find_first_bmp(path, sizeof(path));
        if (!ok) {
            ESP_LOGE(TAG, "no bmp found under %s", sdcard_service_root());
            draw_fill(rgb565(0, 0, 255));
            while (true) vTaskDelay(pdMS_TO_TICKS(1000));
        }
        bmp_list_push(path);
    }

    {
        const char *p = s_bmp_files[s_bmp_file_idx];
        if (p) {
            ESP_LOGI(TAG, "draw: %s", p);
            const esp_err_t ir = draw_bmp(p);
            if (ir != ESP_OK) {
                ESP_LOGE(TAG, "draw bmp failed: %s", esp_err_to_name(ir));
                draw_fill(rgb565(255, 0, 255));
            }
        }
    }

    uint32_t last_click = 0;
    while (true) {
        const uint32_t cur_click = s_key_click_cnt;
        while (last_click != cur_click) {
            last_click++;
            if (s_bmp_file_cnt == 0) continue;
            s_bmp_file_idx = (uint16_t)(((uint32_t)s_bmp_file_idx + 1U) % (uint32_t)s_bmp_file_cnt);

            const char *p = s_bmp_files[s_bmp_file_idx];
            if (!p) continue;
            ESP_LOGI(TAG, "draw: %s", p);
            const esp_err_t ir = draw_bmp(p);
            if (ir != ESP_OK) {
                ESP_LOGE(TAG, "draw bmp failed: %s", esp_err_to_name(ir));
                draw_fill(rgb565(255, 0, 255));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
