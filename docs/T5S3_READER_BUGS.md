# T5S3 Reader Applet — Bug Investigation Report

## Bug 1: Inverted colors

### Symptom

Background and text/font colors are swapped. Black-on-white reading content appears white-on-black, or vice versa.

### Hypotheses (ranked, most likely first)

1. **Double Inversion:** A previous agent removed a bitwise NOT (`~`) in `src/graphics/niche/Drivers/EInk/ED047TC1Parallel.cpp` thinking it would fix the issue, but InkHUD's `clearBuffer` and `drawPixel` logic already expects `1=WHITE, 0=BLACK`. FastEPD hardware expects `0=WHITE, 1=BLACK`. Removing the inversion in the driver likely caused the current inverted state or was a response to an incorrect InkHUD state.
2. **InkHUD Inversion Setting:** The `config.display.displaymode` is set to `INVERTED`, causing `Renderer::runOnce` to invert the entire buffer.

### Evidence gathered

- `src/graphics/niche/InkHUD/InkHUD.h`: `BLACK = 0, WHITE = 1`.
- `src/graphics/niche/InkHUD/Renderer.cpp`: `clearBuffer()` uses `memset(..., 0xFF, ...)`, meaning White = 1 (all bits set).
- `src/graphics/niche/Drivers/EInk/ED047TC1Parallel.cpp`: Bitwise NOT (`~`) was removed in a previous turn by this agent (or similar logic was altered). FastEPD usually expects 0 for white.
- `ReaderApplet.cpp`: Uses `Color::WHITE` for `fillRect` (background) and `Color::BLACK` for text. This matches InkHUD convention.

### Confirmed root cause

Inconsistency between InkHUD logical colors and `ED047TC1Parallel` driver polarity. InkHUD uses `1` for white. FastEPD hardware uses `0` for white. The driver MUST invert the buffer to bridge these two conventions.

### Fix options

A. Restore bitwise NOT (`~`) in `ED047TC1Parallel.cpp`.
B. Change InkHUD color definitions (not recommended, touches core).

### Recommended fix

**Option A.** Restore the inversion in the driver shim.

---

## Bug 2: Sizing too small

### Symptom

Everything on screen (font, margins, UI chrome) is smaller than appropriate for a 960×540 panel.

### Hypotheses (ranked, most likely first)

1. **Low-resolution font selection:** The `setupNicheGraphics` in `variants/esp32s3/t5s3_epaper/nicheGraphics.h` sets `fontLarge` to 12pt, which is tiny on a 960x540 display compared to typical 128x64 or 250x122 displays where InkHUD was born.
2. **Tiling constraint:** The applet is restricted to a small tile (see Bug 3).
3. **EPD_PADDING:** The padding of 24px reduces logical resolution, but only by 48px total, not enough to explain "too small".

### Evidence gathered

- `nicheGraphics.h`: `InkHUD::Applet::fontLarge = FREESANS_12PT_WIN1252`.
- `ED047TC1Parallel.cpp`: Driver is initialized with 960x540 (or 912x492 with padding).
- ReaderApplet uses `fontMedium` for book list and `printWrapped` (which defaults to `fontSmall`) for reading. `fontSmall` is 6pt.

### Confirmed root cause

The default InkHUD fonts (6pt, 9pt, 12pt) are sized for small OLEDs/E-inks and appear minuscule on the T5S3's high-DPI 4.7" panel.

### Fix options

A. Scale fonts or use larger glyph sets (e.g., 18pt, 24pt).
B. Use a global scale factor in InkHUD (touches core).

### Recommended fix

**Option A.** Define and use larger fonts in `setupNicheGraphics` or specifically within `ReaderApplet`.

---

## Bug 3: Tiled with other applets

### Symptom

The reader is rendering side-by-side with other InkHUD applets when it should own the entire screen as a foreground app.

### Hypotheses (ranked, most likely first)

1. **Default Tile Assignment:** In `setupNicheGraphics`, the reader is added with `onTile=1`, and the layout is set to `count=2`. This explicitly forces it into a split-screen view.
2. **Missing Fullscreen Intent:** `ReaderApplet` does not request exclusive use of the screen or change the layout to 1-tile mode when foregrounded.

### Evidence gathered

- `nicheGraphics.h`: `inkhud->persistence->settings.userTiles.count = 2;`.
- `nicheGraphics.h`: `inkhud->addApplet("Reader", new reader::ReaderApplet, true, false, 1);`.
- `WindowManager.cpp`: `placeUserTiles` uses `setRegion(count, index)`. With count=2, it splits the screen.

### Confirmed root cause

InkHUD is configured for a 2-tile layout by default, and the Reader is assigned to Tile 1. InkHUD applets by default respect their assigned tile boundaries.

### Fix options

A. Set `userTiles.count = 1` in `setupNicheGraphics`.
B. Modify `ReaderApplet` to call `inkhud->persistence->settings.userTiles.count = 1` and `changeLayout()` on foreground (touches settings).
C. Add "Fullscreen" support to `Applet` base (touches core).

### Recommended fix

**Option B (modified).** When the Reader enters `READING` view, it should temporarily set the tile count to 1 and refocus. When exiting, restore to 2.

---

## Cross-bug analysis

Are any of these bugs related?

- **Bug 2 and 3:** Yes. If the applet is in a 2-tile layout, it only has ~456x492 pixels (assuming portrait split). This makes everything look even smaller. Fixing the tiling (Bug 3) will provide more space, but fonts will still be too small for the DPI.

Recommended fix order (and why):

1. **Bug 3 (Tiling):** Establishing full-screen ownership defines the final canvas size for the reader.
2. **Bug 2 (Sizing):** Once full-screen, we can choose appropriate font sizes for the 540x960 canvas.
3. **Bug 1 (Colors):** Independent of layout, can be fixed anytime.

Estimated blast radius of each fix:

- Bug 1: `ED047TC1Parallel.cpp`. Affects all displays using this driver (T5S3).
- Bug 2: `ReaderApplet.cpp` or `nicheGraphics.h`. Low risk.
- Bug 3: `ReaderApplet.cpp` and potentially `WindowManager`. Low-Medium risk (layout transitions).
