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

    // E-Ink Driver
    // -----------------------------

    // Use E-Ink driver
    Drivers::EInk *driver = new Drivers::ED047TC1Parallel;
    driver->begin(nullptr, -1, -1, -1); // Parallel driver doesn't need SPI/DC/CS/BUSY here

    // InkHUD
    // ----------------------------

    InkHUD::InkHUD *inkhud = InkHUD::InkHUD::getInstance();

    // Set the driver
    inkhud->setDriver(driver);

    // Set how many FAST updates per FULL update
    // Set how unhealthy additional FAST updates beyond this number are
    inkhud->setDisplayResilience(7, 1.5);

    // Prepare fonts
    InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1252;
    InkHUD::Applet::fontMedium = FREESANS_9PT_WIN1252;
    InkHUD::Applet::fontSmall = FREESANS_6PT_WIN1252;

    // Init settings, and customize defaults
    inkhud->persistence->settings.userTiles.maxCount = 4; // T5S3 Pro has a big screen!
    inkhud->persistence->settings.rotation = 3;           // 270 degrees clockwise (Portrait)
    inkhud->persistence->settings.userTiles.count = 2;    // Two tiles by default
    inkhud->persistence->settings.optionalMenuItems.nextTile = true;
    inkhud->persistence->settings.optionalFeatures.batteryIcon = true;

    // Setup backlight
    Drivers::LatchingBacklight *backlight = Drivers::LatchingBacklight::getInstance();
    backlight->setPin(BOARD_BL_EN);
    if (uiconfig.screen_brightness > 0) {
        backlight->latch();
    }

    // Pick applets
    // Note: order of applets determines priority of "auto-show" feature
    // Optional arguments for defaults:
    // - is activated?
    // - is autoshown?
    // - is foreground on a specific tile (index)?
    inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true); // Activated, autoshown
    inkhud->addApplet("DMs", new InkHUD::DMApplet);
    inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
    inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
    inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true); // Activated
    inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
    inkhud->addApplet("Heard", new InkHUD::HeardApplet, true, false, 0); // Activated, not autoshown, default on tile 0
#ifdef HAS_READER
    inkhud->addApplet("Reader", new reader::ReaderApplet, true, false, 1); // Activated, not autoshown, tile 1
#endif
    // inkhud->addApplet("Basic", new InkHUD::BasicExampleApplet);
    // inkhud->addApplet("NewMsg", new InkHUD::NewMsgExampleApplet);

    // Start running InkHUD
    inkhud->begin();

    // Buttons
    // --------------------------

    Inputs::TwoButton *buttons = Inputs::TwoButton::getInstance(); // A shared NicheGraphics component

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
}

#endif