// InkHUD setup for LILYGO T-Deck Pro Voice.
// Display: Good Display GDEQ031T10 (240x320 mono e-ink, UC8253 controller).
// Note: PIN_EINK_RES = -1 on this board (no hardware reset wired).

#pragma once

#include "configuration.h"

#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

#include "graphics/niche/InkHUD/InkHUD.h"

// User applets
#include "graphics/niche/InkHUD/Applets/User/AllMessage/AllMessageApplet.h"
#include "graphics/niche/InkHUD/Applets/User/DM/DMApplet.h"
#include "graphics/niche/InkHUD/Applets/User/FavoritesMap/FavoritesMapApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Heard/HeardApplet.h"
#include "graphics/niche/InkHUD/Applets/User/Positions/PositionsApplet.h"
#include "graphics/niche/InkHUD/Applets/User/RecentsList/RecentsListApplet.h"
#include "graphics/niche/InkHUD/Applets/User/ThreadedMessage/ThreadedMessageApplet.h"

// Niche driver + inputs
#include "graphics/niche/Drivers/EInk/GDEQ031T10.h"
#include "graphics/niche/Inputs/TwoButton.h"

// Variant-local input adapters
#include "CST328InkHUDAdapter.h"
#include "TDeckProKeyboardInkHUDAdapter.h"

#include <SPI.h>

void setupNicheGraphics()
{
    using namespace NicheGraphics;

    // SPI bus is shared with the SX1262. By the time setupNicheGraphics() runs
    // (after module + LoRa init), SPI is already begun. Re-binding the same
    // pins is a safe no-op on ESP32 -- ensures the host is ready if init order changes.
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);

    // E-ink driver
    auto *driver = new Drivers::GDEQ031T10();
    driver->begin(&SPI, PIN_EINK_DC, PIN_EINK_CS, PIN_EINK_BUSY, PIN_EINK_RES);

    // InkHUD
    auto *inkhud = InkHUD::InkHUD::getInstance();
    inkhud->setDriver(driver);
    // 10 fast (partial) updates allowed before forcing a full refresh; 1.5x ghosting tolerance.
    // Matches old EINK_LIMIT_FASTREFRESH=10 from the BaseUI variant.
    inkhud->setDisplayResilience(10, 1.5);

    // Fonts
    InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1252;
    InkHUD::Applet::fontMedium = FREESANS_9PT_WIN1252;
    InkHUD::Applet::fontSmall = FREESANS_6PT_WIN1252;

    // Defaults
    inkhud->persistence->settings.userTiles.maxCount = 2;
    inkhud->persistence->settings.userTiles.count = 1;
    inkhud->persistence->settings.rotation = 0;
    inkhud->persistence->settings.optionalFeatures.batteryIcon = true;
    // Joystick mode -- enables nav-event handlers in InkHUD::Events.
    // Keyboard/touch adapters (added later) feed nav events through these.
    inkhud->persistence->settings.joystick.enabled = true;

    // Applet roster (order = autoshow priority)
    inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true);
    inkhud->addApplet("DMs", new InkHUD::DMApplet);
    inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
    inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
    inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true);
    inkhud->addApplet("Favorites Map", new InkHUD::FavoritesMapApplet);
    inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
    inkhud->addApplet("Heard", new InkHUD::HeardApplet, true, false, 0);

    inkhud->begin();

    // GPIO 0 hardware button -> InkHUD short/long press
    auto *buttons = Inputs::TwoButton::getInstance();
    buttons->setWiring(0, BUTTON_PIN);
    buttons->setHandlerShortPress(0, []() { InkHUD::InkHUD::getInstance()->shortpress(); });
    buttons->setHandlerLongPress(0, []() { InkHUD::InkHUD::getInstance()->longpress(); });
    buttons->start();

    // TCA8418 keyboard -> InkHUD nav + free-text
    auto *kb = new TDPV::TDeckProKeyboardInkHUDAdapter();
    kb->begin();

    // CST328 touch -> InkHUD nav (tap zones) + longpress (open menu)
    auto *touch = new TDPV::CST328InkHUDAdapter();
    touch->begin();
}

#endif // MESHTASTIC_INCLUDE_NICHE_GRAPHICS
