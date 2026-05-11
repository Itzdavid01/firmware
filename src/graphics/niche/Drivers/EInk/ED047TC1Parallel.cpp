#include "./ED047TC1Parallel.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "main.h"

#include "variant.h"

using namespace NicheGraphics::Drivers;

#ifdef EPD_PADDING
ED047TC1Parallel::ED047TC1Parallel() : EInk(960 - 2 * EPD_PADDING, 540 - 2 * EPD_PADDING, (UpdateTypes)(FULL | FAST))
{
    xOffset = EPD_PADDING;
    yOffset = EPD_PADDING;
#else
ED047TC1Parallel::ED047TC1Parallel() : EInk(960, 540, (UpdateTypes)(FULL | FAST))
{
#endif
    epaper = new FASTEPD;
}

ED047TC1Parallel::~ED047TC1Parallel()
{
    delete epaper;
}

void ED047TC1Parallel::begin(SPIClass *spi, uint8_t pin_dc, uint8_t pin_cs, uint8_t pin_busy, uint8_t pin_rst)
{
    LOG_INFO("ED047TC1Parallel init");
#if defined(T5_S3_EPAPER_PRO_V1)
    epaper->initPanel(BB_PANEL_LILYGO_T5PRO, 28000000);
#else
    // Default to V2 if not specified, or if T5_S3_EPAPER_PRO_V2 is defined
    epaper->initPanel(BB_PANEL_LILYGO_T5PRO_V2, 28000000);
    // Initialize all port 0 pins as outputs / HIGH (some are used for power control on V2)
    for (int i = 0; i < 8; i++) {
        epaper->ioPinMode(i, OUTPUT);
        epaper->ioWrite(i, HIGH);
    }
#endif

    epaper->setMode(BB_MODE_1BPP);
    epaper->clearWhite();
    epaper->fullUpdate(true);
}

void ED047TC1Parallel::update(uint8_t *imageData, UpdateTypes type)
{
    // Copy the InkHUD buffer to FastEPD's current buffer
    // FastEPD buffer size is (width * height / 8) for 1BPP
    uint8_t *cur = epaper->currentBuffer();
    size_t physBufSize = (physicalWidth * physicalHeight) / 8;
    const uint16_t physRowBytes = physicalWidth / 8;
    const uint16_t logRowBytes = ((width - 1) / 8) + 1;

    // Clear physical buffer (0 = white)
    memset(cur, 0, physBufSize);

    // InkHUD buffer is also horizontal-byte 1BPP
    // But we need to ensure polarity is correct.
    // FastEPD: 1 = black, 0 = white.
    // InkHUD: 1 = white, 0 = black.
    // We NEED to invert it for FastEPD to get a white background (0 -> 1).
    for (uint16_t y = 0; y < height; y++) {
        uint8_t *srcRow = &imageData[y * logRowBytes];
        uint8_t *dstRow = &cur[(y + yOffset) * physRowBytes + (xOffset / 8)];
        for (uint16_t xb = 0; xb < logRowBytes; xb++) {
            dstRow[xb] = ~srcRow[xb];
        }
    }

    if (type == FAST) {
        // Fast refresh
        epaper->fullUpdate(CLEAR_FAST, false);
        beginPolling(50, 500);
    } else {
        // Full refresh
        epaper->fullUpdate(CLEAR_SLOW, false);
        beginPolling(100, 2000);
    }
}

bool ED047TC1Parallel::isUpdateDone()
{
    // FastEPD doesn't expose a simple 'busy' check for the parallel bus
    // because it's usually blocking unless using async extensions.
    // But since we called fullUpdate(..., false), it's supposedly non-blocking?
    // Actually, FastEPD fullUpdate is usually blocking.
    return true;
}

void ED047TC1Parallel::finalizeUpdate()
{
    epaper->backupPlane();
}

#endif
