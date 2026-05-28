/*

Most of the Meshtastic firmware uses preprocessor macros throughout the code to support different hardware variants.
NicheGraphics attempts a different approach:

Per-device config takes place in this setupNicheGraphics() method
(And a small amount in platformio.ini)

This file sets up InkHUD for Heltec VM-E290.
Different NicheGraphics UIs and different hardware variants will each have their own setup procedure.

*/

#pragma once

#include "configuration.h"
#include "mesh/MeshModule.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

// InkHUD-specific components
// ---------------------------
// #include "graphics/niche/InkHUD/InkHUD.h"
#include "graphics/niche/InkHUD/WindowManager.h"

// Applets
#include "graphics/niche/InkHUD/Applets/User/AllMessage/AllMessageApplet.h"
#include "graphics/niche/InkHUD/Applets/User/DM/DMApplet.h"
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

    // Use E-Ink driver
    Drivers::EInk *driver = new Drivers::ED047TC1Parallel;
    driver->begin(nullptr, -1, -1, -1); // Parallel driver doesn't need SPI/DC/CS/BUSY here

    LOG_INFO("setupNicheGraphics: after driver init, free heap: %d", ESP.getFreeHeap());

    // InkHUD
    // ----------------------------

    InkHUD::InkHUD *inkhud = InkHUD::InkHUD::getInstance();

    // Set the driver
    inkhud->setDriver(driver);

    // Set how many FAST updates per FULL update
    // Set how unhealthy additional FAST updates beyond this number are
    inkhud->setDisplayResilience(7, 1.5);

    // Prepare fonts
    InkHUD::Applet::fontLarge = FREESANS_24PT_WIN1252;
    InkHUD::Applet::fontMedium = FREESANS_18PT_WIN1252;
    InkHUD::Applet::fontSmall = FREESANS_12PT_WIN1252;

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
    // inkhud->addApplet("Basic", new InkHUD::BasicExampleApplet);
    // inkhud->addApplet("NewMsg", new InkHUD::NewMsgExampleApplet);

    // Start running InkHUD
    LOG_INFO("setupNicheGraphics: calling inkhud->begin()");
    inkhud->begin();
    LOG_INFO("setupNicheGraphics: inkhud->begin() complete");

    // Buttons
    // --------------------------
    LOG_INFO("setupNicheGraphics: getting TwoButton instance");
    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance();
    LOG_INFO("setupNicheGraphics: TwoButton instance acquired, init observers");
    buttons->initObservers();
    LOG_INFO("setupNicheGraphics: observers initialized");

    // Setup the main user button (0)
    buttons->setWiring(0, BUTTON_PIN);
    buttons->setHandlerShortPress(0, []() { InkHUD::InkHUD::getInstance()->shortpress(); });
    buttons->setHandlerLongPress(0, []() { InkHUD::InkHUD::getInstance()->longpress(); });

#if defined(T5_S3_EPAPER_PRO_V1)
    // Setup the aux button (1)
    // V1 has two buttons
    buttons->setWiring(1, PIN_BUTTON2);
    buttons->setHandlerShortPress(1, []() { InkHUD::InkHUD::getInstance()->nextTile(); });
#endif

    buttons->start();
    LOG_INFO("setupNicheGraphics: complete");
}

#endif