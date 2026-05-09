#include "configuration.h"

#ifdef T5_S3_EPAPER_PRO

#include "TouchDrvGT911.hpp"
#include "Wire.h"
#include "input/TouchScreenImpl1.h"

TouchDrvGT911 touch;

bool readTouch(int16_t *x, int16_t *y)
{
    if (!digitalRead(GT911_PIN_INT)) {
        int16_t raw_x;
        int16_t raw_y;
        if (touch.getPoint(&raw_x, &raw_y)) {
            // rotate 90° for landscape
            *x = raw_y;
            *y = EPD_WIDTH - 1 - raw_x;
            LOG_DEBUG("touched(%d/%d)", *x, *y);
            return true;
        }
    }
    return false;
}

void earlyInitVariant()
{
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(BOARD_BL_EN, OUTPUT);
    digitalWrite(BOARD_BL_EN, LOW); // Keep panel power off until FastEPD needs it

    // GT911 reset sequence to latch address 0x14 (avoid 0x5D SFA30 conflict)
    pinMode(GT911_PIN_RST, OUTPUT);
    digitalWrite(GT911_PIN_RST, LOW);
    pinMode(GT911_PIN_INT, OUTPUT);
    digitalWrite(GT911_PIN_INT, HIGH); // HIGH -> latch 0x14
    delay(1);
    digitalWrite(GT911_PIN_RST, HIGH);
    delay(10);
    pinMode(GT911_PIN_INT, INPUT); // Release for interrupt use

#if !defined(T5_S3_EPAPER_PRO_V1)
    // Enable peripheral power early via PCA9535 Port 0
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

// T5-S3-ePaper Pro specific (late-) init
void lateInitVariant(void)
{
    touch.setPins(GT911_PIN_RST, GT911_PIN_INT);
    if (touch.begin(Wire, GT911_SLAVE_ADDRESS_H, GT911_PIN_SDA, GT911_PIN_SCL)) {
        touchScreenImpl1 = new TouchScreenImpl1(EPD_WIDTH, EPD_HEIGHT, readTouch);
        touchScreenImpl1->init();
    } else {
        LOG_ERROR("Failed to find touch controller!");
    }
}
#endif