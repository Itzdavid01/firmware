#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "TDeckProKeyboardInkHUDAdapter.h"

#include "graphics/niche/InkHUD/InkHUD.h"
// TDeckProKeyboard.h already drags in TCA8418KeyboardBase.h, which lacks
// include guards -- so we MUST NOT include the base header again here.
#include "input/TDeckProKeyboard.h"

#include <Wire.h>

using NicheGraphics::InkHUD::InkHUD;
using Key = TDeckProKeyboard::TCA8418Key;

namespace TDPV
{

TDeckProKeyboardInkHUDAdapter::TDeckProKeyboardInkHUDAdapter() : concurrency::OSThread("TDPVKeyboard") {}

void TDeckProKeyboardInkHUDAdapter::begin()
{
    if (started)
        return;
    kb = new TDeckProKeyboard();
    kb->begin(TCA8418_KB_ADDR, &Wire);
    kb->reset();
    started = true;
    setIntervalFromNow(30);
}

int32_t TDeckProKeyboardInkHUDAdapter::runOnce()
{
    if (!started || !kb)
        return 100;

    // Drive the I2C poll. trigger() reads pending matrix events and feeds
    // pressed()/released(); released() pushes characters onto the queue.
    kb->trigger();

    while (kb->hasEvent()) {
        char c = kb->dequeueEvent();
        if (c == 0)
            continue;
        dispatch(c);
    }

    // Slightly faster than the standard InputBroker poll -- e-ink already debounces.
    return 30;
}

void TDeckProKeyboardInkHUDAdapter::dispatch(char c)
{
    auto *inkhud = InkHUD::getInstance();
    if (!inkhud)
        return;

    switch ((uint8_t)c) {
    case Key::LEFT:
        inkhud->navLeft();
        return;
    case Key::RIGHT:
        inkhud->navRight();
        return;
    case Key::UP:
        inkhud->navUp();
        return;
    case Key::DOWN:
        inkhud->navDown();
        return;
    case Key::SELECT: // Enter
        inkhud->shortpress();
        return;
    case Key::ESC:
        inkhud->freeTextCancel();
        return;
    case Key::TAB:
    case Key::BSP:
        inkhud->freeText(c);
        return;
    case Key::REBOOT:
    case Key::BT_TOGGLE:
    case Key::GPS_TOGGLE:
    case Key::MUTE_TOGGLE:
    case Key::SEND_PING:
    case Key::BL_TOGGLE:
    case Key::TOUCH_LOCK:
    case Key::READ_ALOUD:
    case Key::FUNCTION_F1:
    case Key::FUNCTION_F2:
    case Key::FUNCTION_F3:
    case Key::FUNCTION_F4:
    case Key::FUNCTION_F5:
        // Function keys not handled in this first cut. Drop silently.
        return;
    default:
        // Printable ASCII -> free-text channel. Consumed only when a system
        // applet (e.g. KeyboardApplet) wants it; otherwise harmlessly dropped.
        if (c >= 0x20 && c < 0x7F)
            inkhud->freeText(c);
        return;
    }
}

} // namespace TDPV

#endif // MESHTASTIC_INCLUDE_INKHUD
