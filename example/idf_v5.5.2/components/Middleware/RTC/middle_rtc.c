#include "middle_rtc.h"
#include "bsp/esp32_s3_matrix.h"
#include "esp_check.h"
#include "esp_timer.h"

static const char *TAG = "middle_rtc";
static bool inited = false;
static volatile uint32_t alarm_seq = 0;
static pcf85063a_dev_t rtc_dev;
static pcf85063a_datetime_t cache_time;
static int64_t cache_ts_us = 0;

static bool rtc_is_leap_year(uint16_t year)
{
    bool d4 = (year % 4U) == 0U;
    bool d100 = (year % 100U) == 0U;
    bool d400 = (year % 400U) == 0U;
    return (d4 && !d100) || d400;
}

static uint8_t rtc_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t dim[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    uint8_t base = (month >= 1 && month <= 12) ? dim[month - 1] : 31;
    if (month != 2) return base;
    if (!rtc_is_leap_year(year)) return base;
    return 29;
}

static void rtc_add_seconds(pcf85063a_datetime_t *t, uint32_t add_sec)
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
    while (day_total > rtc_days_in_month(t->year, t->month)) {
        day_total -= rtc_days_in_month(t->year, t->month);
        t->month = (uint8_t)(t->month + 1U);
        if (t->month <= 12) continue;
        t->month = 1;
        t->year = (uint16_t)(t->year + 1U);
    }
    t->day = (uint8_t)day_total;
    t->dotw = (uint8_t)((t->dotw + carry_day) % 7U);
}

esp_err_t middle_rtc_init(void)
{
    if (inited) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp_i2c_init failed");
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;
    ESP_RETURN_ON_ERROR(pcf85063a_init(&rtc_dev, bus, PCF85063A_ADDRESS), TAG, "pcf85063a_init failed");

    inited = true;
    alarm_seq = 0;
    cache_ts_us = 0;
    return ESP_OK;
}

esp_err_t middle_rtc_set_time(pcf85063a_datetime_t t)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    ESP_RETURN_ON_ERROR(pcf85063a_set_time_date(&rtc_dev, t), TAG, "pcf85063a_set_time_date failed");
    cache_time = t;
    cache_ts_us = esp_timer_get_time();
    return ESP_OK;
}

esp_err_t middle_rtc_get_time(pcf85063a_datetime_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    if (!inited) return ESP_ERR_INVALID_STATE;

    const int64_t now_us = esp_timer_get_time(); 
    const bool cache_valid = (cache_ts_us != 0) && ((now_us - cache_ts_us) < 200 * 1000);
    if (cache_valid) {
        *out = cache_time;
        return ESP_OK;
    }
    pcf85063a_datetime_t t;
    ESP_RETURN_ON_ERROR(pcf85063a_get_time_date(&rtc_dev, &t), TAG, "pcf85063a_get_time_date failed");

    uint8_t alarm_flag = 0;
    esp_err_t r = pcf85063a_get_alarm_flag(&rtc_dev, &alarm_flag);
    const bool alarm_hit = (r == ESP_OK) && ((alarm_flag & PCF85063A_RTC_CTRL_2_AF) != 0U);
    if (alarm_hit) {
        alarm_seq++;
        pcf85063a_enable_alarm(&rtc_dev);
    }
    
    cache_time = t;
    cache_ts_us = now_us;
    *out = t;
    return ESP_OK;
}

esp_err_t middle_rtc_alarm(uint32_t after_sec)
{
    if (!inited) return ESP_ERR_INVALID_STATE;
    pcf85063a_datetime_t now;
    ESP_RETURN_ON_ERROR(middle_rtc_get_time(&now), TAG, "middle_rtc_get_time failed");
    rtc_add_seconds(&now, after_sec);
    ESP_RETURN_ON_ERROR(pcf85063a_set_alarm(&rtc_dev, now), TAG, "pcf85063a_set_alarm failed");
    return pcf85063a_enable_alarm(&rtc_dev);
}

uint32_t middle_rtc_alarm_seq(void)
{
    return alarm_seq;
}
