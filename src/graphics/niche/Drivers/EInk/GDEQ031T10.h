// E-Ink driver for Good Display GDEQ031T10 (240x320, UC8253 controller).
// Used by t-deck-pro-voice. Ported from GxEPD2_310_GDEQ031T10 command sequences.

#pragma once

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "configuration.h"

#include "./EInk.h"

namespace NicheGraphics::Drivers
{

class GDEQ031T10 : public EInk
{
  public:
    GDEQ031T10();
    void begin(SPIClass *spi, uint8_t pin_dc, uint8_t pin_cs, uint8_t pin_busy, uint8_t pin_rst = -1) override;
    void update(uint8_t *imageData, UpdateTypes type) override;

  protected:
    void wait(uint32_t timeoutMs = 3000);
    void resetController();
    void initDisplay();
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void sendData(const uint8_t *data, uint32_t size);

    void configFull();
    void configFast();
    void powerOn();
    void powerOff();

    void setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void writeImageBlock(uint8_t command, const uint8_t *image);

    void detachFromUpdate();
    bool isUpdateDone() override;
    void finalizeUpdate() override;

  protected:
    static constexpr uint16_t WIDTH = 240;
    static constexpr uint16_t HEIGHT = 320;
    static constexpr uint8_t BUSY_ACTIVE = LOW; // BUSY pin = LOW when controller busy

    uint16_t bufferRowSize = 0;
    uint32_t bufferSize = 0;
    uint8_t *buffer = nullptr;
    uint8_t *previousBuffer = nullptr;
    bool hasPreviousBuffer = false;
    bool initDone = false;
    bool powerIsOn = false;
    UpdateTypes updateType = UpdateTypes::UNSPECIFIED;

    uint8_t pin_dc = (uint8_t)-1;
    uint8_t pin_cs = (uint8_t)-1;
    uint8_t pin_busy = (uint8_t)-1;
    uint8_t pin_rst = (uint8_t)-1;
    SPIClass *spi = nullptr;
    SPISettings spiSettings = SPISettings(10000000, MSBFIRST, SPI_MODE0);
};

} // namespace NicheGraphics::Drivers

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
