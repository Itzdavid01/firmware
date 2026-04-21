#include "variant.h"
#include "Arduino.h"

void earlyInitVariant()
{
    pinMode(LORA_EN, OUTPUT);
    digitalWrite(LORA_EN, HIGH);
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(PIN_EINK_CS, OUTPUT);
    digitalWrite(PIN_EINK_CS, HIGH);

#ifdef BOARD_1V8_EN
    pinMode(BOARD_1V8_EN, OUTPUT);
    digitalWrite(BOARD_1V8_EN, HIGH);
#endif

#ifdef PIN_DRV_EN
    pinMode(PIN_DRV_EN, OUTPUT);
    digitalWrite(PIN_DRV_EN, HIGH);
#endif

    // Audio circuit power enable (GPIO 41 = BOARD_6609_EN)
    pinMode(41, OUTPUT);
    digitalWrite(41, HIGH);
    // I2S MCLK — ESP32-S3 driver requires valid GPIO even though PCM5102A ignores it
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
}