#include "variant.h"
#include "Arduino.h"

void earlyInitVariant()
{
    // Enable LoRa module
    pinMode(LORA_EN, OUTPUT);
    digitalWrite(LORA_EN, HIGH);

    // Deselect all SPI devices
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(PIN_EINK_CS, OUTPUT);
    digitalWrite(PIN_EINK_CS, HIGH);

    // Initialize vibration motor pin (ensure off at boot)
    pinMode(PIN_VIBRATION, OUTPUT);
    digitalWrite(PIN_VIBRATION, LOW);
}
