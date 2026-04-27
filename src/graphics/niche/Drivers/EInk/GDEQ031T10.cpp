#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "./GDEQ031T10.h"

#include <cstring>

#include "SPILock.h"

using namespace NicheGraphics::Drivers;

// UC8253 controller, GDEQ031T10 panel: 240 x 320 mono.
// Supports FULL and FAST (partial) update.
GDEQ031T10::GDEQ031T10() : EInk(WIDTH, HEIGHT, (UpdateTypes)(FULL | FAST))
{
    bufferRowSize = ((WIDTH - 1) / 8) + 1;         // 30 bytes per row
    bufferSize = bufferRowSize * (uint32_t)HEIGHT; // 9600 bytes
}

void GDEQ031T10::begin(SPIClass *spi, uint8_t pin_dc, uint8_t pin_cs, uint8_t pin_busy, uint8_t pin_rst)
{
    this->spi = spi;
    this->pin_dc = pin_dc;
    this->pin_cs = pin_cs;
    this->pin_busy = pin_busy;
    this->pin_rst = pin_rst;

    pinMode(pin_dc, OUTPUT);
    pinMode(pin_cs, OUTPUT);
    digitalWrite(pin_cs, HIGH);
    pinMode(pin_busy, INPUT);

    if (pin_rst != (uint8_t)-1) {
        pinMode(pin_rst, OUTPUT);
        digitalWrite(pin_rst, HIGH);
    }

    if (!previousBuffer) {
        previousBuffer = new uint8_t[bufferSize];
        if (previousBuffer)
            memset(previousBuffer, 0xFF, bufferSize);
    }
}

void GDEQ031T10::update(uint8_t *imageData, UpdateTypes type)
{
    buffer = imageData;
    updateType = (type == UpdateTypes::UNSPECIFIED) ? UpdateTypes::FULL : type;

    // Skip a FAST update if the framebuffer is identical to the last one shown
    if (updateType == FAST && hasPreviousBuffer && previousBuffer && memcmp(previousBuffer, buffer, bufferSize) == 0)
        return;

    initDisplay();

    if (updateType == FAST) {
        configFast();
        // Partial-window covering the full screen (matches GxEPD2 refresh(true) path)
        sendCommand(0x91); // partial in
        setPartialRamArea(0, 0, WIDTH, HEIGHT);
    } else {
        configFull();
    }

    // Previous frame: for FULL update or first FAST, prime with current buffer
    // (matches GxEPD2 writeImageForFullRefresh / initial_write semantics)
    if (updateType == FAST && hasPreviousBuffer && previousBuffer)
        writeImageBlock(0x10, previousBuffer);
    else
        writeImageBlock(0x10, buffer);

    // Current frame
    writeImageBlock(0x13, buffer);

    powerOn();
    sendCommand(0x12); // display refresh -- async; we'll poll BUSY in detachFromUpdate

    // Cache for next FAST diff. Safe to copy now: SPI tx of buffer already complete.
    if (previousBuffer) {
        memcpy(previousBuffer, buffer, bufferSize);
        hasPreviousBuffer = true;
    }

    detachFromUpdate();
}

void GDEQ031T10::wait(uint32_t timeoutMs)
{
    if (failed)
        return;

    uint32_t started = millis();
    while (digitalRead(pin_busy) == BUSY_ACTIVE) {
        if ((millis() - started) > timeoutMs) {
            failed = true;
            break;
        }
        yield();
    }
}

void GDEQ031T10::resetController()
{
    // No hardware reset on this board (PIN_EINK_RES = -1).
    // UC8253 soft init via PSR command happens inside initDisplay().
    if (pin_rst != (uint8_t)-1) {
        digitalWrite(pin_rst, LOW);
        delay(10);
        digitalWrite(pin_rst, HIGH);
        delay(10);
        wait(3000);
    }
}

void GDEQ031T10::initDisplay()
{
    if (initDone)
        return;

    resetController();

    // Soft reset via PSR (no RST pin available)
    sendCommand(0x00); // PANEL SETTING
    sendData(0x1e);    // soft reset bit
    sendData(0x0d);
    delay(2);

    // Real PSR
    sendCommand(0x00); // PANEL SETTING
    sendData(0x1f);    // KW mode (matches GxEPD2 default)
    sendData(0x0d);

    initDone = true;
    powerIsOn = false;
}

void GDEQ031T10::sendCommand(uint8_t command)
{
    if (failed)
        return;

    spiLock->lock();
    spi->beginTransaction(spiSettings);
    digitalWrite(pin_dc, LOW);
    digitalWrite(pin_cs, LOW);
    spi->transfer(command);
    digitalWrite(pin_cs, HIGH);
    digitalWrite(pin_dc, HIGH);
    spi->endTransaction();
    spiLock->unlock();
}

void GDEQ031T10::sendData(uint8_t data)
{
    sendData(&data, 1);
}

void GDEQ031T10::sendData(const uint8_t *data, uint32_t size)
{
    if (failed)
        return;

    spiLock->lock();
    spi->beginTransaction(spiSettings);
    digitalWrite(pin_dc, HIGH);
    digitalWrite(pin_cs, LOW);

#if defined(ARCH_ESP32)
    spi->transferBytes(data, NULL, size);
#elif defined(ARCH_NRF52)
    spi->transfer(data, NULL, size);
#else
    for (uint32_t i = 0; i < size; ++i)
        spi->transfer(data[i]);
#endif

    digitalWrite(pin_cs, HIGH);
    digitalWrite(pin_dc, HIGH);
    spi->endTransaction();
    spiLock->unlock();
}

void GDEQ031T10::configFull()
{
    // Cascade Setting + force temperature for fast full update (90C bin)
    sendCommand(0xE0);
    sendData(0x02); // TSFIX
    sendCommand(0xE5);
    sendData(0x5A); // 90C, ~1.0s

    // VCOM/data interval setting
    sendCommand(0x50);
    sendData(0x97);
}

void GDEQ031T10::configFast()
{
    // Cascade Setting + force temperature for fast partial (121C bin)
    sendCommand(0xE0);
    sendData(0x02);
    sendCommand(0xE5);
    sendData(0x79);

    sendCommand(0x50);
    sendData(0xD7);
}

void GDEQ031T10::powerOn()
{
    if (powerIsOn)
        return;
    sendCommand(0x04);
    wait(2000);
    powerIsOn = true;
}

void GDEQ031T10::powerOff()
{
    if (!powerIsOn)
        return;
    sendCommand(0x02);
    wait(1500);
    powerIsOn = false;
}

void GDEQ031T10::setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t xe = (x + w - 1) | 0x0007; // byte-aligned end
    uint16_t ye = y + h - 1;
    x &= 0xFFF8;

    sendCommand(0x90); // partial window
    sendData(x);
    sendData(xe);
    sendData(y / 256);
    sendData(y % 256);
    sendData(ye / 256);
    sendData(ye % 256);
    sendData(0x01);
}

void GDEQ031T10::writeImageBlock(uint8_t command, const uint8_t *image)
{
    sendCommand(command);
    sendData(image, bufferSize);
}

void GDEQ031T10::detachFromUpdate()
{
    switch (updateType) {
    case FAST:
        return beginPolling(50, 800);
    case FULL:
    default:
        return beginPolling(100, 1500);
    }
}

bool GDEQ031T10::isUpdateDone()
{
    return digitalRead(pin_busy) != BUSY_ACTIVE;
}

void GDEQ031T10::finalizeUpdate()
{
    // Close the partial-update window if we opened one
    if (updateType == FAST)
        sendCommand(0x92); // partial out

    powerOff();

    // Mark display as needing re-init before next update
    // (matches GxEPD2 _Update_Full / _Update_Part: "_init_display_done = false; reason unknown")
    initDone = false;

    // Skip deep sleep when no RST pin available -- we cannot wake the controller.
    if (pin_rst != (uint8_t)-1) {
        sendCommand(0x07);
        sendData(0xA5);
    }
}

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
