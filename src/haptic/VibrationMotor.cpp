#include "configuration.h"

#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)

#include "VibrationMotor.h"
#include "main.h"
#include <Arduino.h>

VibrationMotor *vibrationMotor = nullptr;

VibrationMotor::VibrationMotor() : initialized(false) {}

VibrationMotor::~VibrationMotor()
{
    deinit();
}

bool VibrationMotor::init()
{
    if (initialized) {
        return true;
    }

#ifdef PIN_VIBRATION
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_VIBRATION, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    initialized = true;
    LOG_INFO("VibrationMotor: initialized on GPIO %d (ch=%d, freq=%dHz)", PIN_VIBRATION, PWM_CHANNEL, PWM_FREQ);
    return true;
#else
    LOG_WARN("VibrationMotor: PIN_VIBRATION not defined");
    return false;
#endif
}

void VibrationMotor::deinit()
{
    if (!initialized) {
        return;
    }

    motorOff();
#ifdef PIN_VIBRATION
    ledcDetachPin(PIN_VIBRATION);
#endif
    initialized = false;
}

void VibrationMotor::pulse(uint16_t duration_ms, uint8_t intensity)
{
    if (!initialized) {
        return;
    }

    motorOn(intensity);
    // Use FreeRTOS delay - safe in dedicated task, yields CPU to other tasks
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    motorOff();
}

void VibrationMotor::pattern(HapticPattern pat)
{
    switch (pat) {
    case HAPTIC_SHORT:
        pulse(50, 200);
        break;

    case HAPTIC_DOUBLE:
        pulse(50, 200);
        vTaskDelay(pdMS_TO_TICKS(80));
        pulse(50, 200);
        break;

    case HAPTIC_LONG:
        pulse(200, 180);
        break;

    case HAPTIC_TRIPLE:
        pulse(40, 200);
        vTaskDelay(pdMS_TO_TICKS(60));
        pulse(40, 200);
        vTaskDelay(pdMS_TO_TICKS(60));
        pulse(40, 200);
        break;
    }
}

void VibrationMotor::motorOn(uint8_t intensity)
{
    if (initialized) {
        ledcWrite(PWM_CHANNEL, intensity);
    }
}

void VibrationMotor::motorOff()
{
    if (initialized) {
        ledcWrite(PWM_CHANNEL, 0);
    }
}

#endif // HAS_VIBRATION_MOTOR && ARCH_ESP32
