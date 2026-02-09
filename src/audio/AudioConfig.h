#pragma once

/*
 * Audio configuration for T-Deck Pro Voice variant.
 * Defines sample rates, buffer sizes, and default volume for the PCM5102A I2S DAC.
 */

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 16000
#endif

// I2S DMA buffer configuration
#define I2S_DMA_BUF_COUNT 8
#define I2S_DMA_BUF_LEN 64

// Volume defaults (0-100 scale)
#define AUDIO_DEFAULT_VOLUME 60
#define AUDIO_MAX_VOLUME 100
#define AUDIO_MIN_VOLUME 0

// Tone generation limits
#define TONE_FREQ_MIN 100
#define TONE_FREQ_MAX 8000
#define TONE_DURATION_MAX_MS 5000
