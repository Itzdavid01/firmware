/*

Most of the Meshtastic firmware uses preprocessor macros throughout the code to support different hardware variants.
NicheGraphics attempts a different approach:

Per-device config takes place in this setupNicheGraphics() method
(And a small amount in platformio.ini)

This file sets up InkHUD for the LilyGo T5-E-Paper-S3-Pro.

The board uses a 4.7" ED047TC1 parallel e-paper display (960×540, 8-bit parallel interface).
This is driven via the FastEPD library through the NicheGraphics ED047TC1 driver adapter.

*/

#pragma once

#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

// InkHUD-specific components
// ---------------------------
#include "graphics/niche/InkHUD/InkHUD.h"

// Applets
#include "graphics/niche/InkHUD/Applets/User/AllMessage/AllMessageApplet.h"
#include "graphics/niche/InkHUD/Applets/User/DM/DMApplet.h"
#include "graphics/niche/InkHUD/Applets/User/FavoritesMap/FavoritesMapApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Heard/HeardApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Positions/PositionsApplet.h"
#include "graphics/niche/InkHUD/Applets/User/RecentsList/RecentsListApplet.h"
#include "graphics/niche/InkHUD/Applets/User/ThreadedMessage/ThreadedMessageApplet.h"
#ifdef HAS_READER
#include "reader/ReaderApplet.h"
#endif

// Shared NicheGraphics components
// --------------------------------
#include "graphics/niche/Drivers/Backlight/LatchingBacklight.h"
#include "graphics/niche/Drivers/EInk/ED047TC1Parallel.h"
#include "graphics/niche/Inputs/TwoButton.h"

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    LOG_INFO("setupNicheGraphics: entry, free heap: %d", ESP.getFreeHeap());

    // E-Ink Driver
    // -----------------------------
    // The ED047TC1 is a parallel display - no SPI bus setup needed.
    // begin() args are part of the EInk interface but are ignored for parallel displays.

    // Use E-Ink driver (parallel ED047TC1 via FastEPD)
    Drivers::EInk *driver = new Drivers::ED047TC1Parallel;
    driver->begin(nullptr, -1, -1, -1); // Parallel driver doesn't need SPI/DC/CS/BUSY here

    LOG_INFO("setupNicheGraphics: after driver init, free heap: %d", ESP.getFreeHeap());

    // InkHUD
    // ----------------------------

    InkHUD::InkHUD *inkhud = InkHUD::InkHUD::getInstance();

    // Set the driver
    inkhud->setDriver(driver);

    // Large 4.7" ED047TC1 panel: FAST updates are full-screen spatially,
    // so FULL refreshes are visually disruptive. Use conservative thresholds
    // to reduce slow FULL refresh frequency and maintenance refreshes.
    inkhud->setDisplayResilience(20, 2.0);

    // Prepare fonts - use larger sizes to suit the 4.7" screen at ~234 DPI
    InkHUD::Applet::fontLarge = FREESANS_24PT_WIN1253;
    InkHUD::Applet::fontMedium = FREESANS_18PT_WIN1253;
    InkHUD::Applet::fontSmall = FREESANS_12PT_WIN1253;

    // Init settings, and customize defaults
    inkhud->persistence->settings.userTiles.maxCount = 4; // T5S3 Pro has a big screen!
    inkhud->persistence->settings.rotation = 3;           // 270 degrees clockwise (Portrait)
    inkhud->persistence->settings.userTiles.count = 1;    // One tile by default for full-screen focus
    inkhud->persistence->settings.optionalMenuItems.nextTile = true;
    inkhud->persistence->settings.optionalFeatures.batteryIcon = true;

    // Setup backlight
    Drivers::LatchingBacklight *backlight = Drivers::LatchingBacklight::getInstance();
    backlight->setPin(BOARD_BL_EN);
    backlight->off();

    // Pick applets
    // Note: order of applets determines priority of "auto-show" feature
    // Optional arguments for defaults:
    // - is activated?
    // - is autoshown?
    // - is foreground on a specific tile (index)?
    LOG_INFO("setupNicheGraphics: before applet 1, free heap: %d", ESP.getFreeHeap());
    inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true); // Activated, autoshown
    LOG_INFO("setupNicheGraphics: after AllMessage, free heap: %d", ESP.getFreeHeap());
    inkhud->addApplet("DMs", new InkHUD::DMApplet);
    LOG_INFO("setupNicheGraphics: after DM, free heap: %d", ESP.getFreeHeap());
    inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
    LOG_INFO("setupNicheGraphics: after ThreadedMessage0, free heap: %d", ESP.getFreeHeap());
    inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
    LOG_INFO("setupNicheGraphics: after ThreadedMessage1, free heap: %d", ESP.getFreeHeap());
    inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true); // Activated
    LOG_INFO("setupNicheGraphics: after Positions, free heap: %d", ESP.getFreeHeap());
    inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
    LOG_INFO("setupNicheGraphics: after RecentsList, free heap: %d", ESP.getFreeHeap());
    inkhud->addApplet("Heard", new InkHUD::HeardApplet, true, false, 0); // Activated, not autoshown, default on tile 0
    LOG_INFO("setupNicheGraphics: after Heard, free heap: %d", ESP.getFreeHeap());
#ifdef HAS_READER
    inkhud->addApplet("Reader", new reader::ReaderApplet, true, false, 0); // Activated, not autoshown, tile 0
    LOG_INFO("setupNicheGraphics: after Reader, free heap: %d", ESP.getFreeHeap());
#endif
    inkhud->addApplet("Favorites Map", new InkHUD::FavoritesMapApplet, false, false); // Not Active, not autoshown
    // inkhud->addApplet("Basic", new InkHUD::BasicExampleApplet);
    // inkhud->addApplet("NewMsg", new InkHUD::NewMsgExampleApplet);

    // Enable reusable InkHUD touch status indicator for this touch-capable board.
    inkhud->setTouchEnabledProvider(isTouchInputEnabled);

    // Start running InkHUD
    LOG_INFO("setupNicheGraphics: calling inkhud->begin()");
    inkhud->begin();
    LOG_INFO("setupNicheGraphics: inkhud->begin() complete");

    // Arm GT911 capacitive-home callback only after InkHUD startup is complete.
    t5SetHomeCapButtonEventsEnabled(true);

    // Keep single-button semantics regardless of persisted settings:
    // short press advances, long press opens menu/selects.
    inkhud->persistence->settings.joystick.enabled = false;

    // Buttons
    // --------------------------
    LOG_INFO("setupNicheGraphics: getting TwoButton instance");
    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance();
    LOG_INFO("setupNicheGraphics: TwoButton instance acquired, init observers");
    buttons->initObservers();
    LOG_INFO("setupNicheGraphics: observers initialized");

    // #0: BOOT button (primary user input for InkHUD navigation on T5-S3)
#if defined(T5_S3_EPAPER_PRO_V1)
    buttons->setWiring(0, PIN_BUTTON2);
#else
    // V2 / standard: BOOT button is on GPIO0 (BUTTON_PIN). Without this the button
    // is never wired, so start() attaches an ISR to an unconfigured pin (gpio_isr
    // error + "IO 0 is not set as GPIO" flood) and the BOOT button never registers.
    buttons->setWiring(0, BUTTON_PIN, true);
#endif
    buttons->setHandlerShortPress(0, [inkhud]() { inkhud->shortpress(); });
    buttons->setHandlerLongPress(0, [inkhud]() { inkhud->longpress(); });

#if defined(T5_S3_EPAPER_PRO_V1)
    // Aux button (1) - V1 has two buttons
    buttons->setWiring(1, PIN_BUTTON2);
    buttons->setHandlerShortPress(1, [inkhud]() { inkhud->nextTile(); });
#endif

    buttons->start();
    LOG_INFO("setupNicheGraphics: complete");
}

#endif
