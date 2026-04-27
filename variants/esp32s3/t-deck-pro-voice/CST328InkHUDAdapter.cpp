#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "CST328InkHUDAdapter.h"

#include "graphics/niche/InkHUD/InkHUD.h"

#include <Arduino.h>

// readTouch() is the shared touch reader defined in
// src/platform/extra_variants/t_deck_pro/variant.cpp -- it abstracts CST328 vs
// CST3530 detection and IRQ vs polling behaviour.
extern bool readTouch(int16_t *x, int16_t *y);

using NicheGraphics::InkHUD::InkHUD;

namespace TDPV
{

static constexpr uint32_t LONG_PRESS_MS = 800;
static constexpr int16_t TAP_SLOP_PX = 20; // movement under this counts as a tap, not a swipe

CST328InkHUDAdapter::CST328InkHUDAdapter() : concurrency::OSThread("TDPVTouch") {}

void CST328InkHUDAdapter::begin()
{
    // CST328 init runs in lateInitVariant() (src/platform/extra_variants/t_deck_pro/variant.cpp).
    // Nothing to (re)init here -- just start polling.
    started = true;
    setIntervalFromNow(40);
}

int32_t CST328InkHUDAdapter::runOnce()
{
    if (!started)
        return 100;

    auto *inkhud = InkHUD::getInstance();
    int16_t x = 0, y = 0;
    bool isDown = readTouch(&x, &y);

    uint32_t now = millis();

    if (isDown && !wasDown) {
        // Touch-down edge
        wasDown = true;
        downX = x;
        downY = y;
        downAtMs = now;
        longFired = false;
    } else if (isDown && wasDown) {
        // Held -- fire longpress once after threshold
        if (!longFired && (now - downAtMs) >= LONG_PRESS_MS) {
            longFired = true;
            if (inkhud)
                inkhud->longpress();
        }
    } else if (!isDown && wasDown) {
        // Touch-up edge -- classify if longpress hasn't already fired
        wasDown = false;
        if (!longFired && inkhud) {
            int16_t dx = (int16_t)abs(x - downX);
            int16_t dy = (int16_t)abs(y - downY);
            if (dx <= TAP_SLOP_PX && dy <= TAP_SLOP_PX) {
                // Tap -- map x to nav zone. Width = 240 (rotation = 0).
                if (downX < 80)
                    inkhud->navLeft();
                else if (downX >= 160)
                    inkhud->navRight();
                else
                    inkhud->shortpress();
            }
        }
    }

    return 40;
}

} // namespace TDPV

#endif // MESHTASTIC_INCLUDE_INKHUD
