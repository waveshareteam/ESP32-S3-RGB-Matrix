#include "middle_audio.h"
#include "bsp/esp32_s3_matrix.h"
#include "esp_codec_dev.h"

static esp_codec_dev_handle_t spk = NULL;
static esp_codec_dev_handle_t mic = NULL;
static bool opened = false;

static bool out_vol_valid = false;
static bool in_gain_valid = false;
static bool out_mute_valid = false;
static uint8_t last_out_vol = 0;
static float last_in_gain = 0;
static bool last_out_mute = false;

static void audio_reset_params(void)
{
    out_vol_valid = false;
    in_gain_valid = false;
    out_mute_valid = false;
    last_out_vol = 0;
    last_in_gain = 0;
    last_out_mute = false;
}

esp_err_t middle_audio_init(bool enable_speaker, bool enable_microphone)
{
    if (!BSP_CAPS_AUDIO) return ESP_ERR_NOT_SUPPORTED;
    if (!enable_speaker && !enable_microphone) return ESP_ERR_INVALID_ARG;

    if (enable_speaker && !spk) {
        spk = bsp_audio_codec_speaker_init();
    }
    if (enable_microphone && !mic) {
        mic = bsp_audio_codec_microphone_init();
    }
    if (enable_speaker && !spk) return ESP_FAIL;
    if (enable_microphone && !mic) return ESP_FAIL;

    opened = false;
    audio_reset_params();
    return ESP_OK;
}

esp_err_t middle_audio_open(int sample_rate, int channel, int bits_per_sample)
{
    if (!spk && !mic) return ESP_ERR_INVALID_STATE;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = sample_rate,
        .channel = channel,
        .bits_per_sample = bits_per_sample,
    };

    bool spk_opened = false;
    if (spk) {
        const int r1 = esp_codec_dev_open(spk, &fs);
        if (r1 != ESP_CODEC_DEV_OK) return ESP_FAIL;
        spk_opened = true;
    }
    if (mic) {
        const int r2 = esp_codec_dev_open(mic, &fs);
        if (r2 != ESP_CODEC_DEV_OK) {
            if (spk_opened) {
                esp_codec_dev_close(spk);
            }
            return ESP_FAIL;
        }
    }

    opened = true;
    audio_reset_params();
    return ESP_OK;
}

esp_err_t middle_audio_set_out_vol(uint8_t vol)
{
    if (!spk || !opened) return ESP_ERR_INVALID_STATE;

    if (out_vol_valid && vol == last_out_vol) return ESP_OK;
    const int r = esp_codec_dev_set_out_vol(spk, vol);
    if (r != ESP_CODEC_DEV_OK) return ESP_FAIL;

    last_out_vol = vol;
    out_vol_valid = true;
    return ESP_OK;
}

esp_err_t middle_audio_set_in_gain(float gain)
{
    if (!mic || !opened) return ESP_ERR_INVALID_STATE;

    if (in_gain_valid && gain == last_in_gain) return ESP_OK;
    const int r = esp_codec_dev_set_in_gain(mic, gain);
    if (r != ESP_CODEC_DEV_OK) return ESP_FAIL;

    last_in_gain = gain;
    in_gain_valid = true;
    return ESP_OK;
}

esp_err_t middle_audio_set_out_mute(bool mute)
{
    if (!spk || !opened) return ESP_ERR_INVALID_STATE;

    if (out_mute_valid && mute == last_out_mute) return ESP_OK;
    const int r = esp_codec_dev_set_out_mute(spk, mute);
    if (r != ESP_CODEC_DEV_OK) return ESP_FAIL;

    last_out_mute = mute;
    out_mute_valid = true;
    return ESP_OK;
}

esp_err_t middle_audio_write(const void *data, size_t len)
{
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if (!spk || !opened) return ESP_ERR_INVALID_STATE;
    const int r = esp_codec_dev_write(spk, (void *)data, len);
    if (r != ESP_CODEC_DEV_OK) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t middle_audio_read_i2s(void *data, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (!data || len == 0 || !bytes_read) return ESP_ERR_INVALID_ARG;
    if (!mic || !opened) return ESP_ERR_INVALID_STATE;

    *bytes_read = 0;

    const int r = esp_codec_dev_read(mic, data, (int)len);
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
