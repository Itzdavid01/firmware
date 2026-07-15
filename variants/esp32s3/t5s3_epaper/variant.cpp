// Pin-level early init only. All touch, backlight, and InkHUD code lives in
// src/platform/extra_variants/t5s3_epaper/variant.cpp where PlatformIO's
// library dependency finder can resolve headers like TouchDrvGT911.hpp.
#include "variant.h"
#include "Arduino.h"
#include "Wire.h"
#include "pins_arduino.h"

void earlyInitVariant()
{
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(BOARD_BL_EN, OUTPUT);
    digitalWrite(BOARD_BL_EN, LOW); // Keep panel power off until FastEPD needs it

    // Program GT911 touch controller to I2C address 0x14 (GT911_SLAVE_ADDRESS_H) before
    // the I2C bus scan runs.  GPIO3 (INT) defaults LOW on ESP32-S3 cold boot, which would
    // leave the GT911 at 0x5D (GT911_SLAVE_ADDRESS_L) - the same address as the SFA30
    // air quality sensor - causing a false-positive SFA30 detection during the I2C scan.
    //
    // GT911 datasheet §4.3 "Address Selection":
    //   Pull INT HIGH before releasing RST → device latches address 0x14 (SLAVE_ADDRESS_H)
    //   Pull INT LOW  before releasing RST → device latches address 0x5D (SLAVE_ADDRESS_L)
    //   Minimum RST assert time: 100 µs; minimum startup time after RST deassert: 5 ms.
    //
    // lateInitVariant() calls touch.begin() which repeats this sequence internally while
    // also performing full I2C initialisation; the double-reset is harmless.
    pinMode(GT911_PIN_RST, OUTPUT);
    digitalWrite(GT911_PIN_RST, LOW);
    pinMode(GT911_PIN_INT, OUTPUT);
    digitalWrite(GT911_PIN_INT, HIGH); // HIGH -> latch 0x14
    delay(1);
    digitalWrite(GT911_PIN_RST, HIGH);
    delay(10);
    pinMode(GT911_PIN_INT, INPUT); // Release for interrupt use

#if !defined(T5_S3_EPAPER_PRO_V1)
    // Enable peripheral power early via PCA9535 Port 0 (required for the SD card power rail).
    Wire.begin(GT911_PIN_SDA, GT911_PIN_SCL);
    Wire.beginTransmission(0x20); // PCA9535 ADDR
    Wire.write(0x02);             // REG_OUTPUT_PORT0
    Wire.write(0xFF);             // Enable all power
    Wire.endTransmission();

    Wire.beginTransmission(0x20);
    Wire.write(0x06); // REG_CONFIG_PORT0
    Wire.write(0x00); // All outputs
    Wire.endTransmission();
#endif
}

// Touch / backlight late-init now lives in
// src/platform/extra_variants/t5s3_epaper/variant.cpp (lateInitVariant there).
