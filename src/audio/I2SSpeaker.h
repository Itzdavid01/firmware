#pragma once

#include "configuration.h"

#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)

#include <driver/i2s.h>
#include <stdint.h>

#include "AudioConfig.h"

/**
 * I2S Speaker driver for the PCM5102A DAC on the T-Deck Pro Voice variant.
 *
 * Provides tone generation via sine wave synthesis over I2S.
 * All playback methods are BLOCKING and must only be called from a
 * dedicated FreeRTOS task (see NotificationManager), never from the
 * main cooperative loop or handleReceived().
 */
class I2SSpeaker
{
  public:
    enum BeepPattern {
        BEEP_SHORT,     // 100ms @ 1000Hz
        BEEP_DOUBLE,    // Two short beeps
        BEEP_ASCENDING, // 3 tones: 800 -> 1000 -> 1200Hz
        BEEP_WARNING    // Low 400Hz for 200ms
    };

    I2SSpeaker();
    ~I2SSpeaker();

    /// Initialize the I2S hardware. Safe to call at boot.
    bool init();

    /// Deinitialize I2S hardware.
    void deinit();

    /// Play a single tone. BLOCKING - only call from dedicated task.
    void playTone(uint16_t freq_hz, uint16_t duration_ms);

    /// Play a predefined beep pattern. BLOCKING - only call from dedicated task.
    void playBeep(BeepPattern pattern);

    /// Set volume (0-100).
    void setVolume(uint8_t volume);
    uint8_t getVolume() const { return currentVolume; }

    void stop();

  private:
    bool initialized;
    uint8_t currentVolume;
    i2s_port_t i2sPort;

    void writeSineWave(uint16_t freq_hz, uint16_t duration_ms);
    void writeSilence(uint16_t duration_ms);
};

extern I2SSpeaker *i2sSpeaker;

#endif // HAS_I2S_SPEAKER && ARCH_ESP32
