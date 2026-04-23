#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * @brief Initialize audio service
 * @param enable_speaker Whether to enable speaker output
 * @param enable_microphone Whether to enable microphone input
 * @return esp_err_t Error code
 */
esp_err_t middle_audio_init(bool enable_speaker, bool enable_microphone); 

/*
 * @brief Open audio service and configure sample rate, channels, and bit depth
 * @param sample_rate Sample rate
 * @param channel Number of channels
 * @param bits_per_sample Bit depth
 * @return esp_err_t Error code
 */
esp_err_t middle_audio_open(int sample_rate, int channel, int bits_per_sample);

esp_err_t middle_audio_set_out_vol(uint8_t vol); // Set output volume (range: 0–100)

esp_err_t middle_audio_set_in_gain(float gain);  // Set input gain (range: 0.0–1.0)

esp_err_t middle_audio_set_out_mute(bool mute);  // Set output mute (true = mute)

/*
 * @brief Write audio data to I2S
 * @param data Data buffer
 * @param len Data length
 * @return esp_err_t Error code
 */
esp_err_t middle_audio_write(const void *data, size_t len);

/*
 * @brief Read audio data from I2S
 * @param data Data buffer
 * @param len Data length
 * @param bytes_read Number of bytes actually read
 * @param timeout_ms Timeout in milliseconds
 * @return esp_err_t Error code
 */
esp_err_t middle_audio_read_i2s(void *data, size_t len, size_t *bytes_read, uint32_t timeout_ms);
