#pragma once

#include "esp_err.h"
#include "pcf85063a.h"
#include <stdint.h>

esp_err_t middle_rtc_init(void); // Initialize RTC service

esp_err_t middle_rtc_set_time(pcf85063a_datetime_t t); // Set RTC time

esp_err_t middle_rtc_get_time(pcf85063a_datetime_t *out); // Get current RTC time

esp_err_t middle_rtc_alarm(uint32_t after_sec); // Set an alarm after a specified delay (in seconds)

uint32_t middle_rtc_alarm_seq(void); // Get the alarm sequence number
