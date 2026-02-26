#include "rtc_service.h"

#include "extra_bsp.h"

#include "esp_attr.h"
#include "esp_timer.h"

static bool s_inited = false;
static volatile uint32_t s_alarm_seq = 0;

static pcf85063a_datetime_t s_cache_time;
static int64_t s_cache_ts_us = 0;

static void IRAM_ATTR rtc_service_irq(void *arg)
{
    (void)arg;
    s_alarm_seq++;
}

static bool rtc_service_is_leap_year(uint16_t year)
{
    bool d4 = (year % 4U) == 0U;
    bool d100 = (year % 100U) == 0U;
    bool d400 = (year % 400U) == 0U;
    return (d4 && !d100) || d400;
}

static uint8_t rtc_service_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t dim[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    uint8_t base = (month >= 1 && month <= 12) ? dim[month - 1] : 31;
    if (month != 2) return base;
    if (!rtc_service_is_leap_year(year)) return base;
    return 29;
}

static void rtc_service_add_seconds(pcf85063a_datetime_t *t, uint32_t add_sec)
{
    if (!t) return;

    uint32_t sec_total = (uint32_t)t->sec + add_sec;
    uint32_t carry_min = sec_total / 60U;
    uint32_t min_total = (uint32_t)t->min + carry_min;
    uint32_t carry_hour = min_total / 60U;
    uint32_t hour_total = (uint32_t)t->hour + carry_hour;
    uint32_t carry_day = hour_total / 24U;

    t->sec = (uint8_t)(sec_total % 60U);
    t->min = (uint8_t)(min_total % 60U);
    t->hour = (uint8_t)(hour_total % 24U);

    uint32_t day_total = (uint32_t)t->day + carry_day;
    while (day_total > rtc_service_days_in_month(t->year, t->month)) {
        day_total -= rtc_service_days_in_month(t->year, t->month);
        t->month = (uint8_t)(t->month + 1U);
        if (t->month <= 12) continue;
        t->month = 1;
        t->year = (uint16_t)(t->year + 1U);
    }
    t->day = (uint8_t)day_total;
    t->dotw = (uint8_t)((t->dotw + carry_day) % 7U);
}

esp_err_t rtc_service_init(void)
{
    if (s_inited) return ESP_OK;

    esp_err_t r = bsp_init_pcf85063a();
    if (r != ESP_OK) return r;

    r = bsp_rtc_int_register(rtc_service_irq, NULL);
    if (r != ESP_OK) return r;

    s_inited = true;
    s_cache_ts_us = 0;
    return ESP_OK;
}

esp_err_t rtc_service_set_time(pcf85063a_datetime_t t)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    esp_err_t r = bsp_rtc_set_time_date(t);
    if (r != ESP_OK) return r;
    s_cache_time = t;
    s_cache_ts_us = esp_timer_get_time();
    return ESP_OK;
}

esp_err_t rtc_service_get_time(pcf85063a_datetime_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    const int64_t now_us = esp_timer_get_time();
    const bool cache_valid = (s_cache_ts_us != 0) && ((now_us - s_cache_ts_us) < 200 * 1000);
    if (cache_valid) {
        *out = s_cache_time;
        return ESP_OK;
    }

    pcf85063a_datetime_t t;
    esp_err_t r = bsp_rtc_get_time_date(&t);
    if (r != ESP_OK) return r;

    s_cache_time = t;
    s_cache_ts_us = now_us;
    *out = t;
    return ESP_OK;
}

esp_err_t rtc_service_arm_alarm_after(uint32_t after_sec)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    pcf85063a_datetime_t now;
    esp_err_t r = rtc_service_get_time(&now);
    if (r != ESP_OK) return r;

    rtc_service_add_seconds(&now, after_sec);
    r = bsp_rtc_set_alarm(now);
    if (r != ESP_OK) return r;

    r = bsp_rtc_enable_alarm();
    if (r != ESP_OK) return r;
    return ESP_OK;
}

uint32_t rtc_service_alarm_seq(void)
{
    return s_alarm_seq;
}
