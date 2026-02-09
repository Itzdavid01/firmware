#pragma once

#include "configuration.h"

#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)

#include <driver/i2s.h>
#include <stdint.h>

#include "AudioConfig.h"

/**
 * I2S Speaker driver for the PCM5102A DAC on the T-Deck Pro Voice variant.
 *
 * Provides low-level tone generation via sine wave synthesis over I2S.
 * Uses DMA for non-blocking playback. The driver manages the I2S peripheral
 * directly and should not be used simultaneously with AudioThread/ESP8266Audio.
 */
class I2SSpeaker
{
  public:
    enum BeepPattern {
        BEEP_SHORT,    // 100ms @ 1000Hz
        BEEP_DOUBLE,   // Two short beeps
        BEEP_ASCENDING, // 3 tones: 800 -> 1000 -> 1200Hz
        BEEP_WARNING   // Low 400Hz for 200ms
    };

    I2SSpeaker();
    ~I2SSpeaker();

    /// Initialize the I2S hardware. Returns true on success.
    bool init();

    /// Deinitialize I2S hardware to save power.
    void deinit();

    /// Play a single tone at the given frequency and duration.
    void playTone(uint16_t freq_hz, uint16_t duration_ms);

    /// Play a predefined beep pattern.
    void playBeep(BeepPattern pattern);

    /// Set volume (0-100). Affects amplitude of generated tones.
    void setVolume(uint8_t volume);

    /// Get current volume.
    uint8_t getVolume() const { return currentVolume; }

    /// Check if currently playing.
    bool isPlaying() const { return playing; }

    /// Stop any current playback.
    void stop();

  private:
    bool initialized;
    bool playing;
    uint8_t currentVolume;
    i2s_port_t i2sPort;

    /// Write a sine wave buffer to the I2S DMA.
    void writeSineWave(uint16_t freq_hz, uint16_t duration_ms);

    /// Write silence to flush the DMA buffers (prevents pops).
    void writeSilence(uint16_t duration_ms);
};

extern I2SSpeaker *i2sSpeaker;

#endif // HAS_I2S_SPEAKER && ARCH_ESP32
