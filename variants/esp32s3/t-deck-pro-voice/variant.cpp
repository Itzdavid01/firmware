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

    // Try other likely power pins for T-Deck Pro Voice
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    pinMode(41, OUTPUT);
    digitalWrite(41, HIGH);
    pinMode(42, OUTPUT);
    digitalWrite(42, HIGH);
    pinMode(39, OUTPUT);
    digitalWrite(39, HIGH);
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
}