#pragma once

#include "configuration.h"

#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)

#include <stdint.h>

/**
 * PWM-based vibration motor driver for the T-Deck Pro Voice variant.
 *
 * Uses the ESP32 LEDC peripheral for PWM intensity control.
 * Supports predefined haptic patterns with configurable intensity.
 */
class VibrationMotor
{
  public:
    enum HapticPattern {
        HAPTIC_SHORT,  // 50ms pulse
        HAPTIC_DOUBLE, // Two 50ms pulses with 80ms gap
        HAPTIC_LONG,   // 200ms pulse
        HAPTIC_TRIPLE  // Three 40ms pulses
    };

    VibrationMotor();
    ~VibrationMotor();

    /// Initialize PWM on the vibration motor pin. Returns true on success.
    bool init();

    /// Release PWM resources.
    void deinit();

    /// Single pulse with given duration and intensity (0-255).
    void pulse(uint16_t duration_ms, uint8_t intensity = 200);

    /// Play a predefined haptic pattern.
    void pattern(HapticPattern pat);

    /// Check if initialized.
    bool isInitialized() const { return initialized; }

  private:
    bool initialized;

    static const uint8_t PWM_CHANNEL = 4; // LEDC channel (avoid conflicts with other peripherals)
    static const uint32_t PWM_FREQ = 1000; // 1kHz (above audible buzz threshold)
    static const uint8_t PWM_RESOLUTION = 8; // 8-bit (0-255)

    void motorOn(uint8_t intensity);
    void motorOff();
};

extern VibrationMotor *vibrationMotor;

#endif // HAS_VIBRATION_MOTOR && ARCH_ESP32
