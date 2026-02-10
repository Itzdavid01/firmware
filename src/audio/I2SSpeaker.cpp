#include "configuration.h"

#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)

#include "I2SSpeaker.h"
#include "main.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

I2SSpeaker *i2sSpeaker = nullptr;

I2SSpeaker::I2SSpeaker() : initialized(false), currentVolume(AUDIO_DEFAULT_VOLUME), i2sPort(I2S_NUM_0) {}

I2SSpeaker::~I2SSpeaker()
{
    deinit();
}

bool I2SSpeaker::init()
{
    if (initialized) {
        return true;
    }

    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = AUDIO_SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = I2S_DMA_BUF_COUNT;
    i2s_config.dma_buf_len = I2S_DMA_BUF_LEN;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = true;

    esp_err_t err = i2s_driver_install(i2sPort, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        LOG_ERROR("I2SSpeaker: failed to install I2S driver: %d", err);
        return false;
    }

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_SPEAKER_BCLK;
    pin_config.ws_io_num = I2S_SPEAKER_LRC;
    pin_config.data_out_num = I2S_SPEAKER_DOUT;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;

    err = i2s_set_pin(i2sPort, &pin_config);
    if (err != ESP_OK) {
        LOG_ERROR("I2SSpeaker: failed to set I2S pins: %d", err);
        i2s_driver_uninstall(i2sPort);
        return false;
    }

    i2s_zero_dma_buffer(i2sPort);

    initialized = true;
    LOG_INFO("I2SSpeaker: initialized (BCLK=%d, DOUT=%d, LRC=%d, rate=%d)", I2S_SPEAKER_BCLK, I2S_SPEAKER_DOUT, I2S_SPEAKER_LRC,
             AUDIO_SAMPLE_RATE);
    return true;
}

void I2SSpeaker::deinit()
{
    if (!initialized) {
        return;
    }

    stop();
    i2s_zero_dma_buffer(i2sPort);
    i2s_driver_uninstall(i2sPort);
    initialized = false;
}

void I2SSpeaker::playTone(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!initialized) {
        return;
    }

    if (freq_hz < TONE_FREQ_MIN || freq_hz > TONE_FREQ_MAX) {
        return;
    }

    if (duration_ms > TONE_DURATION_MAX_MS) {
        duration_ms = TONE_DURATION_MAX_MS;
    }

    writeSineWave(freq_hz, duration_ms);
    writeSilence(5);
}

void I2SSpeaker::playBeep(BeepPattern pattern)
{
    // All delays use vTaskDelay (FreeRTOS) - safe in a dedicated task
    switch (pattern) {
    case BEEP_SHORT:
        playTone(1000, 100);
        break;

    case BEEP_DOUBLE:
        playTone(1000, 80);
        vTaskDelay(pdMS_TO_TICKS(60));
        playTone(1000, 80);
        break;

    case BEEP_ASCENDING:
        playTone(800, 100);
        vTaskDelay(pdMS_TO_TICKS(30));
        playTone(1000, 100);
        vTaskDelay(pdMS_TO_TICKS(30));
        playTone(1200, 120);
        break;

    case BEEP_WARNING:
        playTone(400, 200);
        break;
    }
}

void I2SSpeaker::setVolume(uint8_t volume)
{
    if (volume > AUDIO_MAX_VOLUME) {
        volume = AUDIO_MAX_VOLUME;
    }
    currentVolume = volume;
}

void I2SSpeaker::stop()
{
    if (initialized) {
        i2s_zero_dma_buffer(i2sPort);
    }
}

void I2SSpeaker::writeSineWave(uint16_t freq_hz, uint16_t duration_ms)
{
    const uint32_t totalSamples = (uint32_t)AUDIO_SAMPLE_RATE * duration_ms / 1000;
    const float amplitude = 32767.0f * ((float)currentVolume / 100.0f);
    const float phaseIncrement = 2.0f * (float)M_PI * (float)freq_hz / (float)AUDIO_SAMPLE_RATE;
    const uint32_t rampSamples = AUDIO_SAMPLE_RATE * 5 / 1000; // 5ms ramp to avoid clicks

    int16_t buffer[I2S_DMA_BUF_LEN];
    uint32_t samplesWritten = 0;
    float phase = 0.0f;

    while (samplesWritten < totalSamples) {
        uint32_t chunkSize = totalSamples - samplesWritten;
        if (chunkSize > I2S_DMA_BUF_LEN) {
            chunkSize = I2S_DMA_BUF_LEN;
        }

        for (uint32_t i = 0; i < chunkSize; i++) {
            float sample = sinf(phase) * amplitude;

            uint32_t globalIdx = samplesWritten + i;
            if (globalIdx < rampSamples) {
                sample *= (float)globalIdx / (float)rampSamples;
            }

            uint32_t samplesRemaining = totalSamples - globalIdx;
            if (samplesRemaining < rampSamples) {
                sample *= (float)samplesRemaining / (float)rampSamples;
            }

            buffer[i] = (int16_t)sample;
            phase += phaseIncrement;
            if (phase >= 2.0f * (float)M_PI) {
                phase -= 2.0f * (float)M_PI;
            }
        }

        size_t bytesWritten = 0;
        i2s_write(i2sPort, buffer, chunkSize * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(100));
        samplesWritten += chunkSize;
    }
}

void I2SSpeaker::writeSilence(uint16_t duration_ms)
{
    const uint32_t totalSamples = (uint32_t)AUDIO_SAMPLE_RATE * duration_ms / 1000;
    int16_t buffer[I2S_DMA_BUF_LEN];
    memset(buffer, 0, sizeof(buffer));

    uint32_t samplesWritten = 0;
    while (samplesWritten < totalSamples) {
        uint32_t chunkSize = totalSamples - samplesWritten;
        if (chunkSize > I2S_DMA_BUF_LEN) {
            chunkSize = I2S_DMA_BUF_LEN;
        }

        size_t bytesWritten = 0;
        i2s_write(i2sPort, buffer, chunkSize * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(100));
        samplesWritten += chunkSize;
    }
}

#endif // HAS_I2S_SPEAKER && ARCH_ESP32
