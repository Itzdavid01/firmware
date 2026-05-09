# src/graphics/niche/InkHUD — E-Ink UI Framework

**Event-driven applet-based e-ink UI.** Separate PlatformIO config: `src/graphics/niche/InkHUD/PlatformioConfig.ini`.

## Architecture

```
InkHUD/
├── InkHUD.h/.cpp           # Singleton mediator (Renderer, WindowManager, Events, Persistence)
├── Renderer.h/.cpp        # OSThread-based async display updater
├── WindowManager.h/.cpp    # Tile/applet multiplexing
├── Events.h                # Input event handler
├── Persistence.h            # Flash storage
├── Applet.h/.cpp           # Base applet class
├── Tile.h/.cpp             # Pixel tile (Applet→Renderer pipeline)
└── Drivers/                 # E-Ink controller drivers
    ├── SSD16XX.cpp         # 8 Solomon Systech drivers
    ├── UC8175.cpp           # UltraChip driver
    └── LCMEN2R13EFC1.cpp   # Custom implementation
```

## Applet Lifecycle

```cpp
class MyApplet : public Applet {
    activate()       → onActivate()
    foreground()    → onForeground() / onBackground()
    render()        → onRender() → draws to assigned Tile
    deactivate()    → onDeactivate()
};
```

## Rendering Pipeline

```
Applet.onRender() → Tile.handleAppletPixel() → Renderer.handlePixel()
    → imageBuffer → driver.update(FULL|FAST) → display
```

## Input Mask System

Applets declare input interest via bitmask:

```cpp
enum InputMask {
    BUTTON_SHORT = 1, BUTTON_LONG = 2,
    EXIT_SHORT = 4, EXIT_LONG = 8,
    NAV_UP = 16, NAV_DOWN = 32, NAV_LEFT = 64, NAV_RIGHT = 128
};
setInputsSubscribed(uint8_t input, bool captured);
```

## Two Applet Tiers

| Tier         | Examples                          | Tile Assignment                  |
| ------------ | --------------------------------- | -------------------------------- |
| SystemApplet | Notification, Tips                | Fixed tiles, special positioning |
| UserApplet   | DM, ThreadedMessage, FavoritesMap | Configurable tiles               |

## Flash Persistence

Settings in `/prefs/inkhud_settings.json` with version migration:

```cpp
static constexpr uint32_t SETTINGS_VERSION = 3;
// Version mismatch → use defaults
```

## Coordinate System

Normalized 0..1.0 coordinates via:

```cpp
uint16_t X(float f); // Map to tile width
uint16_t Y(float f); // Map to tile height
void setCrop(int16_t left, int16_t top, uint16_t width, uint16_t height);
```

## E-Ink Update Types

- **FULL**: Clean refresh, slow, no ghosting
- **FAST**: Partial refresh, faster, may have ghosting
- Stress-tracking prevents display damage from excessive FULL updates

## Events System

Uses `CallbackObserver` for system events:

```cpp
CallbackObserver<Events, void *>(this, &Events::beforeDeepSleep);
CallbackObserver<Events, const meshtastic_MeshPacket *>(
    this, &Events::onReceiveTextMessage);
```

## Variant Configuration

Per-variant `nicheGraphics.h` configures:

- Default applets
- Tile layout
- E-Ink driver selection
