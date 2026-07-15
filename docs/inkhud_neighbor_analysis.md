# InkHUD Configuration Analysis: T-Deck-Pro Voice vs T5S3 e-Paper

**Date:** 2026-05-12
**Source:** Neighbor repo branch `tdeck-pro-voice-poc`

---

## Black Background Question

**InkHUD does NOT have a built-in black background mode.**

The framework uses a standard monochrome palette:

- `BLACK = 0` (pixel off / white on display)
- `WHITE = 1` (pixel on / black on display)

The clear buffer fills with `0xFF` (WHITE). However, there IS an invert option:

```cpp
// In Renderer.cpp - buffer inversion at render time:
if (config.display.displaymode == meshtastic_Config_DisplayConfig_DisplayMode_INVERTED) {
    for (size_t i = 0; i < imageBufferWidth * imageBufferHeight; ++i) {
        imageBuffer[i] = ~imageBuffer[i];
    }
}
```

This inverts the entire buffer at render time, effectively giving a black background with white text. The setting is controlled via `config.display.displaymode` and can be toggled in the menu via `MenuAction::TOGGLE_INVERT_COLOR`.

---

## Key Configuration Differences

### Display Resilience (fastPerFull / stressMultiplier)

| Variant              | fastPerFull | stressMultiplier |
| -------------------- | ----------- | ---------------- |
| **T-Deck-Pro-Voice** | `10`        | `1.5`            |
| **T5S3 e-Paper**     | `7`         | `1.5`            |

T-Deck-Pro-Voice allows more consecutive fast updates before forcing a full refresh.

### Rotation

| Variant              | rotation | Degrees                   |
| -------------------- | -------- | ------------------------- |
| **T-Deck-Pro-Voice** | `0`      | 0° (native)               |
| **T5S3 e-Paper**     | `3`      | 270° clockwise (portrait) |

### Tile Configuration

| Variant              | maxCount | defaultCount |
| -------------------- | -------- | ------------ |
| **T-Deck-Pro-Voice** | `2`      | `1`          |
| **T5S3 e-Paper**     | `4`      | `2`          |

T5S3 has a larger screen and defaults to showing 2 tiles; T-Deck-Pro-Voice shows 1 tile.

### Joystick Mode

| Variant              | joystick.enabled        |
| -------------------- | ----------------------- |
| **T-Deck-Pro-Voice** | `true`                  |
| **T5S3 e-Paper**     | Not set (false/default) |

T-Deck-Pro-Voice explicitly enables joystick mode for navigation event handling.

---

## Features Unique to T-Deck-Pro-Voice

1. **Input Adapters** (custom variant-local code):
   - `CST328InkHUDAdapter.cpp/h` - Touch screen navigation (tap zones) + longpress for menu
   - `TDeckProKeyboardInkHUDAdapter.cpp/h` - TCA8418 keyboard navigation + free-text input

2. **FavoritesMap** applet included in roster

3. **No backlight** - T-Deck-Pro-Voice has no backlight control (e-ink only)

4. **SPI sharing** - Explicit `SPI.begin()` call for shared LoRa SPI bus:

   ```cpp
   SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);
   ```

5. **Button handling** - Single button (GPIO 0) only, no aux button

---

## Features Unique to T5S3 e-Paper

1. **Backlight control** via `LatchingBacklight`:

   ```cpp
   Drivers::LatchingBacklight *backlight = Drivers::LatchingBacklight::getInstance();
   backlight->setPin(BOARD_BL_EN);
   backlight->off();
   ```

2. **Reader Applet** (`HAS_READER` conditional) for e-reader functionality

3. **Aux button support** (V1 hardware only):

   ```cpp
   #if defined(T5_S3_EPAPER_PRO_V1)
   buttons->setWiring(1, PIN_BUTTON2);
   buttons->setHandlerShortPress(1, []() { InkHUD::InkHUD::getInstance()->nextTile(); });
   #endif
   ```

4. **Debug logging** - Extensive `LOG_INFO()` calls throughout setup for heap monitoring

5. **Parallel display driver** - Uses `ED047TC1Parallel` instead of SPI

---

## Applet Roster Comparison

### T-Deck-Pro-Voice (8 applets)

```cpp
inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true);
inkhud->addApplet("DMs", new InkHUD::DMApplet);
inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true);
inkhud->addApplet("Favorites Map", new InkHUD::FavoritesMapApplet);
inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
inkhud->addApplet("Heard", new InkHUD::HeardApplet, true, false, 0);
```

### T5S3 e-Paper (7 applets + Reader)

```cpp
inkhud->addApplet("All Messages", new InkHUD::AllMessageApplet, true, true);
inkhud->addApplet("DMs", new InkHUD::DMApplet);
inkhud->addApplet("Channel 0", new InkHUD::ThreadedMessageApplet(0));
inkhud->addApplet("Channel 1", new InkHUD::ThreadedMessageApplet(1));
inkhud->addApplet("Positions", new InkHUD::PositionsApplet, true);
inkhud->addApplet("Recents List", new InkHUD::RecentsListApplet);
inkhud->addApplet("Heard", new InkHUD::HeardApplet, true, false, 0);
#ifdef HAS_READER
inkhud->addApplet("Reader", new reader::ReaderApplet, true, false, 1);
#endif
```

---

## PlatformIO Build Flags Comparison

### T-Deck-Pro-Voice (`t-deck-pro-voice-inkhud`)

```ini
build_flags =
  ${esp32s3_base.build_flags}
  ${inkhud.build_flags}
  -I variants/esp32s3/t-deck-pro-voice
  -D T_DECK_PRO
  -D T_DECK_PRO_VOICE
  -D EINK_WIDTH=240
  -D EINK_HEIGHT=320
  ; refresh limits / dynamic-display flags handled by InkHUD instead of BaseUI
```

### T5S3 e-Paper (`t5s3_epaper_inkhud`)

```ini
build_flags =
  ${t5s3_epaper_base.build_flags}
  ${inkhud.build_flags}
  -D SDCARD_USE_SPI1
  -D T5_S3_EPAPER_PRO_V2
```

Note: T5S3 base flags include parallel display configuration (`USE_EINK_PARALLELDISPLAY`), EPD timing parameters, and touch threshold settings.

---

## Driver Differences

| Aspect           | T-Deck-Pro-Voice | T5S3 e-Paper             |
| ---------------- | ---------------- | ------------------------ |
| **Driver class** | `GDEQ031T10`     | `ED047TC1Parallel`       |
| **Controller**   | UC8253           | N/A (parallel interface) |
| **Interface**    | SPI              | 8-bit parallel           |
| **Resolution**   | 240x320          | 960x540                  |
| **SPI speed**    | 10 MHz           | N/A                      |
| **Reset pin**    | `-1` (not wired) | `-1` (not used)          |

---

## Files in T-Deck-Pro-Voice Not Present in T5S3

- `CST328InkHUDAdapter.cpp`
- `CST328InkHUDAdapter.h`
- `TDeckProKeyboardInkHUDAdapter.cpp`
- `TDeckProKeyboardInkHUDAdapter.h`

---

## Recommendations for Black Background on T5S3

If black background is desired on T5S3:

1. **Use invert mode** - Set `config.display.displaymode = meshtastic_Config_DisplayConfig_DisplayMode_INVERTED` (user toggleable via menu)

2. **Modify clearBuffer()** - Change `memset(imageBuffer, 0xFF, ...)` to `memset(imageBuffer, 0x00, ...)` in `Renderer.cpp` line 263 (requires InkHUD code modification)

3. **Per-applet** - Modify individual applet `onRender()` methods to use inverted colors (swap BLACK/WHITE usage)
