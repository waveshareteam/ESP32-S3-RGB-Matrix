#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t audio_service_init(void);
esp_err_t audio_service_open(int sample_rate, int channel, int bits_per_sample);

esp_err_t audio_service_set_out_vol(uint8_t vol);
esp_err_t audio_service_set_in_gain(float gain);
esp_err_t audio_service_set_out_mute(bool mute);

esp_err_t audio_service_write(const void *data, size_t len);
esp_err_t audio_service_read_i2s(void *data, size_t len, size_t *bytes_read, uint32_t timeout_ms);

