#include "PCA9535ButtonThread.h"
#include "configuration.h"
#include "meshUtils.h"

PCA9535ButtonThread *pca9535Buttons = nullptr;

PCA9535ButtonThread::PCA9535ButtonThread() : concurrency::OSThread("PCA9535Buttons")
{
    // Configure Port 0 (Peripheral Power)
    // T5S3 Pro V2 uses Port 0 for enabling power to modules:
    // P0.0 = GPS Power
    // P0.1 = LoRa Power
    // P0.2 = SD Power?
    // P0.3 = VCC_3V3_EN
    // P0.4 = EPD_BUSY?
    // Set all Port 0 outputs HIGH to enable power
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(REG_OUTPUT_PORT0);
    Wire.write(0xFF); // Enable all
    Wire.endTransmission();

    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(REG_CONFIG_PORT0);
    Wire.write(0x00); // All outputs
    Wire.endTransmission();

    // Configure Port 1
    // LilyGo T5S3 Pro V2 uses Port 1 for buttons and backlight:
    // P1.0 (10) = Backlight EN
    // P1.1 (11) = Backlight PWM/Brightness
    // P1.2 (12) = PWR button
    // P1.3 (13) = Button 1
    // P1.4 (14) = Button 2

    // Set outputs LOW first
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(REG_OUTPUT_PORT1);
    Wire.write(0x00); // Backlight OFF
    Wire.endTransmission();

    // Set directions (0 = Output, 1 = Input)
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(REG_CONFIG_PORT1);
    Wire.write(0xFC); // 11111100 -> P1.0, P1.1 Outputs, rest Inputs
    Wire.endTransmission();
}

void PCA9535ButtonThread::setBacklight(bool on)
{
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(REG_OUTPUT_PORT1);
    Wire.write(on ? 0x03 : 0x00); // Turn P1.0 and P1.1 HIGH or LOW
    Wire.endTransmission();
}

int32_t PCA9535ButtonThread::runOnce()

{
    // Poll every 50ms
    Wire.beginTransmission(PCA9535_ADDR);
    Wire.write(REG_INPUT_PORT1);
    if (Wire.endTransmission() != 0) {
        return 1000; // I2C error, retry later
    }

    if (Wire.requestFrom(PCA9535_ADDR, (uint8_t)1) != 1) {
        return 1000;
    }

    uint8_t currentState = Wire.read();

    // If we read 0x00, it's highly likely an I2C bus glitch (all buttons pressed at once is nearly impossible)
    if (currentState == 0x00) {
        return 20;
    }

    // Bits that changed from HIGH to LOW (pressed)
    uint8_t changed = (lastPort1State ^ currentState) & lastPort1State;

    if (changed != 0 && (millis() - lastDebounceTime) > 150) {
        lastDebounceTime = millis();
        InputEvent evt;
        evt.source = "PCA9535";
        evt.kbchar = 0;
        evt.touchX = 0;
        evt.touchY = 0;

        // P1.2 (Bit 2) is PWR button -> Map to BACK
        if (changed & (1 << 2)) {
            evt.inputEvent = INPUT_BROKER_ALT_PRESS;
            this->notifyObservers(&evt);
            LOG_DEBUG("PCA9535: PWR Button Pressed");
        }

        // P1.3 (Bit 3) is Button 1 -> Map to Backlight Toggle
        if (changed & (1 << 3)) {
            evt.inputEvent = INPUT_BROKER_BACKLIGHT_TOGGLE;
            this->notifyObservers(&evt);
            LOG_DEBUG("PCA9535: Button 1 Pressed (Backlight Toggle)");
        }
    }

    lastPort1State = currentState;
    return 20; // Poll every 20ms for better responsiveness
}
