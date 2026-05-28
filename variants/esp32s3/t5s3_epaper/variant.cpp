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

    // GT911 reset sequence to latch address 0x14 (avoid 0x5D SFA30 conflict).
    // INT HIGH before releasing RST → device latches 0x14 (GT911_SLAVE_ADDRESS_H).
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
