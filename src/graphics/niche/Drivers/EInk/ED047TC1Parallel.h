#pragma once

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS
#include "configuration.h"

#include "./EInk.h"
#include "FastEPD.h"

namespace NicheGraphics::Drivers
{

class ED047TC1Parallel : public EInk
{
  public:
    ED047TC1Parallel();
    ~ED047TC1Parallel();

    void begin(SPIClass *spi, uint8_t pin_dc, uint8_t pin_cs, uint8_t pin_busy, uint8_t pin_rst = -1) override;
    void update(uint8_t *imageData, UpdateTypes type) override;

  protected:
    bool isUpdateDone() override;
    void finalizeUpdate() override;

  private:
    FASTEPD *epaper = nullptr;
    uint32_t lastUpdateMs = 0;
    uint16_t physicalWidth = 960;
    uint16_t physicalHeight = 540;
    uint16_t xOffset = 0;
    uint16_t yOffset = 0;
};

} // namespace NicheGraphics::Drivers

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
