#include "audio_service.h"

#include "bsp/esp32_s3_matrix.h"

#include "esp_codec_dev.h"

static esp_codec_dev_handle_t s_spk = NULL;
static esp_codec_dev_handle_t s_mic = NULL;
static bool s_opened = false;

static bool s_params_valid = false;
static uint8_t s_last_out_vol = 0;
static float s_last_in_gain = 0;
static bool s_last_out_mute = false;

static void audio_service_reset_params(void)
{
    s_params_valid = false;
    s_last_out_vol = 0;
    s_last_in_gain = 0;
    s_last_out_mute = false;
}

esp_err_t audio_service_init(void)
{
    if (s_spk && s_mic) return ESP_OK;

    s_spk = bsp_audio_codec_speaker_init();
    s_mic = bsp_audio_codec_microphone_init();
    if (!s_spk || !s_mic) return ESP_FAIL;

    s_opened = false;
    audio_service_reset_params();
    return ESP_OK;
}

esp_err_t audio_service_open(int sample_rate, int channel, int bits_per_sample)
{
    if (!s_spk || !s_mic) return ESP_ERR_INVALID_STATE;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = sample_rate,
        .channel = channel,
        .bits_per_sample = bits_per_sample,
    };

    const int r1 = esp_codec_dev_open(s_spk, &fs);
    if (r1 != ESP_CODEC_DEV_OK) return ESP_FAIL;

    const int r2 = esp_codec_dev_open(s_mic, &fs);
    if (r2 != ESP_CODEC_DEV_OK) {
        esp_codec_dev_close(s_spk);
        return ESP_FAIL;
    }

    s_opened = true;
    audio_service_reset_params();
    return ESP_OK;
}

esp_err_t audio_service_set_out_vol(uint8_t vol)
{
    if (!s_spk || !s_opened) return ESP_ERR_INVALID_STATE;

    if (s_params_valid && vol == s_last_out_vol) return ESP_OK;
    const int r = esp_codec_dev_set_out_vol(s_spk, vol);
    if (r != ESP_CODEC_DEV_OK) return ESP_FAIL;

    s_last_out_vol = vol;
    s_params_valid = true;
    return ESP_OK;
}

esp_err_t audio_service_set_in_gain(float gain)
{
    if (!s_mic || !s_opened) return ESP_ERR_INVALID_STATE;

    if (s_params_valid && gain == s_last_in_gain) return ESP_OK;
    const int r = esp_codec_dev_set_in_gain(s_mic, gain);
    if (r != ESP_CODEC_DEV_OK) return ESP_FAIL;

    s_last_in_gain = gain;
    s_params_valid = true;
    return ESP_OK;
}

esp_err_t audio_service_set_out_mute(bool mute)
{
    if (!s_spk || !s_opened) return ESP_ERR_INVALID_STATE;

    if (s_params_valid && mute == s_last_out_mute) return ESP_OK;
    const int r = esp_codec_dev_set_out_mute(s_spk, mute);
    if (r != ESP_CODEC_DEV_OK) return ESP_FAIL;

    s_last_out_mute = mute;
    s_params_valid = true;
    return ESP_OK;
}

esp_err_t audio_service_write(const void *data, size_t len)
{
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if (!s_spk || !s_opened) return ESP_ERR_INVALID_STATE;
    const int r = esp_codec_dev_write(s_spk, (void *)data, len);
    if (r <= 0) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t audio_service_read_i2s(void *data, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (!data || len == 0 || !bytes_read) return ESP_ERR_INVALID_ARG;
    if (!s_mic || !s_opened) return ESP_ERR_INVALID_STATE;

    *bytes_read = 0;

    const int r = esp_codec_dev_read(s_mic, data, (int)len);
    if (r == ESP_CODEC_DEV_OK) {
        *bytes_read = len;
        return ESP_OK;
    }
    if (r == ESP_CODEC_DEV_INVALID_ARG) return ESP_ERR_INVALID_ARG;
    if (r == ESP_CODEC_DEV_WRONG_STATE) return ESP_ERR_INVALID_STATE;
    if (r == ESP_CODEC_DEV_NOT_SUPPORT) return ESP_ERR_NOT_SUPPORTED;
    if (r == ESP_CODEC_DEV_NOT_FOUND) return ESP_ERR_NOT_FOUND;
    return ESP_FAIL;
}
