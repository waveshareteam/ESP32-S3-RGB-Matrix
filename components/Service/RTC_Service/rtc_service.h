#pragma once

#include "esp_err.h"

#include "pcf85063a.h"

#include <stdint.h>

esp_err_t rtc_service_init(void);
esp_err_t rtc_service_set_time(pcf85063a_datetime_t t);
esp_err_t rtc_service_get_time(pcf85063a_datetime_t *out);
esp_err_t rtc_service_arm_alarm_after(uint32_t after_sec);
uint32_t rtc_service_alarm_seq(void);

