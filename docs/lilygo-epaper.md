# LilyGO e-Paper Display Reference

## Overview

LilyGO manufactures a comprehensive lineup of e-paper (E-Ink) development boards combining low-power displays with LoRa radios, ESP32/nRF52 microcontrollers, and various sensors. These boards are popular for Meshtastic deployments due to their excellent outdoor readability, extended battery life, and integrated wireless capabilities.

This document catalogs LilyGO's e-paper hardware specifications, driver ICs, and Meshtastic firmware support. For general e-paper subsystem architecture, see [epaper-display-subsystem.md](epaper-display-subsystem.md).

---

## Display Lineup

### T5 Series (ESP32-Based)

The T5 series is LilyGO's flagship e-paper lineup, built around ESP32 microcontrollers with integrated LoRa and various display sizes.

#### T5 V2.3 / V2.4 (2.13 inch)

| Specification       | Value                      |
| ------------------- | -------------------------- |
| **Display Size**    | 2.13 inch                  |
| **Resolution**      | 212 × 104 pixels           |
| **Driver IC**       | SSD1680                    |
| **Interface**       | SPI                        |
| **Colors**          | Black/White                |
| **Full Refresh**    | 2–8 seconds                |
| **Partial Refresh** | ~0.3 seconds               |
| **MCU**             | ESP32 (WiFi + Bluetooth)   |
| **LoRa**            | SX1262 (optional variants) |

The T5 V2.3/V2.4 is the entry point into LilyGO's e-paper ecosystem. The small 2.13 inch display provides basic status information and works well for nodes that prioritize battery life over display real estate. The SSD1680 controller supports partial refresh for faster updates of changing content.

#### T5s (2.7 inch)

| Specification    | Value            |
| ---------------- | ---------------- |
| **Display Size** | 2.7 inch         |
| **Resolution**   | 264 × 176 pixels |
| **Driver IC**    | SSD1680 variant  |
| **Interface**    | SPI              |
| **Colors**       | Black/White      |
| **Full Refresh** | ~6 seconds       |
| **MCU**          | ESP32            |
| **LoRa**         | SX1262           |

The T5s offers a larger 2.7 inch display with improved readability over the 2.13 inch models. The increased resolution provides space for more detailed status information, maps, or message previews. The SSD1680-based controller maintains compatibility with existing software while offering a bigger canvas.

#### T5 4.7 inch (ESP32-S3)

| Specification    | Value                           |
| ---------------- | ------------------------------- |
| **Display Size** | 4.7 inch                        |
| **Resolution**   | 960 × 540 pixels                |
| **Driver IC**    | ED047TC1 (custom)               |
| **Interface**    | 8-bit Parallel                  |
| **Colors**       | 16-level grayscale              |
| **Full Refresh** | ~1 second                       |
| **MCU**          | ESP32-S3 (WiFi 4 + Bluetooth 5) |
| **LoRa**         | SX1262                          |
| **Special**      | Requires FastEPD library        |

The T5 4.7 inch represents a significant leap in display technology. Unlike the SPI-based smaller displays, this panel uses an 8-bit parallel interface and supports 16-level grayscale. The larger screen enables rich user interfaces with detailed maps and message threading. The parallel interface requires the FastEPD library rather than GxEPD2.

#### T5 E-Paper S3 Pro

| Specification    | Value                    |
| ---------------- | ------------------------ |
| **Display Size** | 4.7 inch                 |
| **Resolution**   | 960 × 540 pixels         |
| **Driver IC**    | ED047TC1 (custom)        |
| **Interface**    | 8-bit Parallel           |
| **Colors**       | 16-level grayscale       |
| **MCU**          | ESP32-S3                 |
| **LoRa**         | SX1262                   |
| **GPS**          | U-blox or Quectel module |
| **Touch**        | Capacitive touch panel   |
| **SD Card**      | MicroSD slot             |

The T5 E-Paper S3 Pro is LilyGO's premium e-paper offering. It combines the large 4.7 inch grayscale display with LoRa, GPS, touch input, and SD card storage. This configuration is ideal for portable mesh communicators with mapping capabilities and extended storage for offline maps or logs.

---

### T3 Series

#### T-LoRa T3-S3 (T3S3) e-Paper

| Specification    | Value            |
| ---------------- | ---------------- |
| **Display Size** | 2.13 inch        |
| **Resolution**   | 250 × 122 pixels |
| **Driver IC**    | DEPG0213BN       |
| **Interface**    | SPI              |
| **Colors**       | Black/White      |
| **MCU**          | ESP32-S3         |
| **LoRa**         | SX1262           |

The T3S3 e-paper variant (also known as T-LoRa T3-S3) combines the newer ESP32-S3 microcontroller with a 2.13 inch e-paper display. The DEPG0213BN driver provides slightly higher resolution (250×122) than the T5 V2.x series (212×104). This board offers a good balance of modern MCU features with compact display size.

---

### T-Echo Series (nRF52840-Based)

#### T-Echo (Standard / Plus / Lite)

| Specification    | Value                                        |
| ---------------- | -------------------------------------------- |
| **Display Size** | 1.54 inch                                    |
| **Resolution**   | 200 × 200 pixels                             |
| **Driver IC**    | SSD1681 or compatible                        |
| **Interface**    | SPI                                          |
| **Colors**       | Black/White/Red (tri-color on some variants) |
| **MCU**          | nRF52840 (Bluetooth 5, NFC)                  |
| **LoRa**         | SX1262                                       |
| **GPS**          | U-blox module                                |
| **Battery**      | 3.7V LiPo with charging                      |

The T-Echo is manufactured by Makerfabs in collaboration with LilyGO, featuring Nordic's nRF52840 instead of ESP32. This provides native Bluetooth 5 support and lower power consumption. The 1.54 inch square display has a unique 200×200 resolution. Some variants support tri-color (black/white/red) displays, though Meshtastic typically uses the black/white mode for faster refresh.

Key advantages of the nRF52840 platform:

- Lower standby power consumption than ESP32
- Native Bluetooth 5 and NFC
- Better power efficiency for battery-operated nodes
- No WiFi (reduces power draw, simpler RF environment)

---

### Mini e-Paper Series

#### Mini e-Paper S3

| Specification    | Value                 |
| ---------------- | --------------------- |
| **Display Size** | 1.02 inch             |
| **Resolution**   | 128 × 80 pixels       |
| **Driver IC**    | GDEW0102T4 compatible |
| **Interface**    | SPI                   |
| **MCU**          | ESP32-S3              |

The Mini e-Paper S3 is an ultra-compact variant designed for wearable applications or space-constrained installations. The tiny 1.02 inch display shows basic status information while maintaining minimal power draw and physical footprint.

---

## Driver IC Reference

LilyGO e-paper displays use several different controller ICs depending on the panel size and generation.

### SPI-Based Controllers

| Driver IC      | Used In           | Resolution       | Notes                                                  |
| -------------- | ----------------- | ---------------- | ------------------------------------------------------ |
| **SSD1680**    | T5 V2.3/V2.4, T5s | 212×104, 264×176 | Common 2.13 inch controller, partial refresh support   |
| **DEPG0213BN** | T3S3              | 250×122          | DKE (Display Engineering) variant, slightly higher res |
| **SSD1681**    | T-Echo            | 200×200          | 1.54 inch square panel, low power                      |
| **SSD1682**    | Various           | Various          | Newer generation, improved refresh                     |
| **UC8175**     | Some 2.13 inch    | Various          | UltraChip controller, timing differs from SSD16xx      |

### Parallel/Grayscale Controllers

| Driver IC    | Used In                | Resolution | Notes                                                 |
| ------------ | ---------------------- | ---------- | ----------------------------------------------------- |
| **ED047TC1** | T5 4.7 inch, T5 S3 Pro | 960×540    | Custom parallel controller, 16-gray, FastEPD required |

The ED047TC1 is unique among LilyGO displays in using an 8-bit parallel interface rather than SPI. This enables the higher refresh rates and grayscale support needed for the large 4.7 inch panel. The parallel interface requires different driver software (FastEPD) compared to the SPI displays.

---

## Meshtastic Support

### Supported Variants

The following Meshtastic firmware variants target LilyGO e-paper hardware:

#### ESP32-S3 Variants

| Variant                    | Hardware                  | Display Driver   | UI Stack         |
| -------------------------- | ------------------------- | ---------------- | ---------------- |
| `t5s3_epaper`              | T5 E-Paper S3 Pro (v1/v2) | ED047TC1Parallel | FastEPD          |
| `t5s3_epaper_inkhud`       | T5 E-Paper S3 Pro         | ED047TC1Parallel | InkHUD           |
| `tlora_t3s3_epaper`        | T-LoRa T3-S3 e-Paper      | GxEPD2_213_BN    | GxEPD2 + Dynamic |
| `tlora_t3s3_epaper-inkhud` | T-LoRa T3-S3 e-Paper      | GxEPD2_213_BN    | InkHUD           |
| `mini-epaper-s3`           | Mini e-Paper S3           | GxEPD2_102       | GxEPD2           |
| `my_esp32s3_diy_eink`      | DIY ESP32-S3 e-ink        | User defined     | GxEPD2           |

#### ESP32 Variants (Classic)

| Variant | Hardware     | Notes                  |
| ------- | ------------ | ---------------------- |
| `t5_v2` | T5 V2.3/V2.4 | Original T5 series     |
| `t5s`   | T5s 2.7 inch | Larger display variant |

#### nRF52840 Variants

| Variant       | Hardware        | Display   | Notes            |
| ------------- | --------------- | --------- | ---------------- |
| `t-echo`      | T-Echo Standard | 1.54 inch | Full features    |
| `t-echo-plus` | T-Echo Plus     | 1.54 inch | Enhanced variant |
| `t-echo-lite` | T-Echo Lite     | 1.54 inch | Basic variant    |

### Key Source Files

When working with LilyGO e-paper support in the Meshtastic firmware:

| File                                                   | Purpose                                  |
| ------------------------------------------------------ | ---------------------------------------- |
| `src/graphics/EInkDisplay2.cpp`                        | SPI e-ink via GxEPD2 (T3S3, T-Echo)      |
| `src/graphics/EInkParallelDisplay.cpp`                 | Parallel 8-bit via FastEPD (T5 4.7 inch) |
| `src/graphics/niche/Drivers/EInk/ED047TC1Parallel.cpp` | Dedicated T5 4.7 inch driver             |
| `variants/esp32s3/t5s3_epaper/`                        | T5 S3 Pro variant definition             |
| `variants/esp32s3/tlora_t3s3_epaper/`                  | T3S3 e-paper variant definition          |
| `variants/nrf52840/t-echo/`                            | T-Echo variant definition                |
| `variants/esp32s3/mini-epaper-s3/`                     | Mini e-paper S3 variant                  |

---

## Build Flags

When compiling Meshtastic for LilyGO e-paper hardware, the following build flags configure the display subsystem.

### Basic E-Ink Enable

```ini
-D USE_EINK                    # Enable e-ink display support via GxEPD2
```

### Display Model Selection (GxEPD2 SPI)

```ini
-D EINK_DISPLAY_MODEL=GxEPD2_213_BN      # T3S3 (DEPG0213BN)
-D EINK_DISPLAY_MODEL=GxEPD2_213_FC1     # Some Heltec variants
-D EINK_DISPLAY_MODEL=GxEPD2_290_BN8     # 2.9 inch panels
-D EINK_DISPLAY_MODEL=GxEPD2_102         # Mini e-paper (128×80)
```

### Display Dimensions

```ini
-D EINK_WIDTH=250              # Panel width in pixels
-D EINK_HEIGHT=122             # Panel height in pixels
```

Common dimension combinations:

- T3S3: 250 × 122
- T5 V2.x: 212 × 104
- T5s: 264 × 176
- T-Echo: 200 × 200
- T5 4.7 inch: 960 × 540

### Refresh Behavior (Dynamic Display)

```ini
-D USE_EINK_DYNAMICDISPLAY     # Enable smart refresh policy
-D EINK_LIMIT_FASTREFRESH=20   # Max consecutive fast refreshes before full
-D EINK_BACKGROUND_USES_FAST   # Use fast refresh for background updates
-D EINK_HASQUIRK_GHOSTING      # Panel prone to ghosting
-D EINK_LIMIT_GHOSTING_PX=5000 # Pixel change threshold to force full refresh
-D EINK_FORCE_DISPLAY_THROTTLE_MS=1000  # Min interval between updates
```

### Parallel Display (T5 4.7 inch)

```ini
-D USE_EINK_PARALLELDISPLAY    # Use FastEPD instead of GxEPD2
-D HAS_EINK_ASYNCFULL          # Enable async full refresh (forked GxEPD2)
```

The parallel display flags are specific to the T5 4.7 inch and T5 S3 Pro boards. These cannot use the standard SPI driver.

### Pin Definitions (variant.h)

Standard e-ink pin naming in variant definitions:

```cpp
#define PIN_EINK_CS     5       // Chip Select
#define PIN_EINK_DC     6       // Data/Command
#define PIN_EINK_RES    7       // Reset
#define PIN_EINK_BUSY   8       // Busy status
#define PIN_EINK_SCLK   3       // SPI Clock
#define PIN_EINK_MOSI   4       // SPI MOSI
#define PIN_EINK_EN     9       // Power enable (optional)
```

Some variants use `hspi` (HSPI bus) instead of default SPI:

- T3S3 e-paper
- Mini e-Paper S3

---

## Low-Power Notes

E-paper displays offer significant power advantages for battery-operated Meshtastic nodes.

### Display Power Characteristics

| Operation                        | Power Draw | Duration     |
| -------------------------------- | ---------- | ------------ |
| **Idle (display holding image)** | ~0 mW      | Indefinite   |
| **Partial refresh**              | ~10–20 mW  | 0.3–1 second |
| **Full refresh**                 | ~30–50 mW  | 2–8 seconds  |

Unlike LCD or OLED displays, e-paper requires zero power to maintain an image. Power is only consumed during refresh operations.

### Optimization Strategies

1. **Minimize refresh frequency**: Update the display only when necessary. Use `BACKGROUND` frame flags for infrequent updates.

2. **Use partial refresh**: For text updates on supported panels, partial refresh uses less power and completes faster than full refresh.

3. **Power down between updates**: Some variants support powering off the display controller between refreshes. Check for `PIN_EINK_EN` support.

4. **Match variant to use case**:
   - Fixed nodes: Any variant works
   - Portable use: T-Echo (nRF52840) offers best battery life
   - Display-heavy use: T5 4.7 inch for readability, accepting higher refresh power

### Deep Sleep Considerations

When the ESP32 or nRF52 enters deep sleep:

- The e-paper display retains its image without power
- Display state is lost (must be re-initialized on wake)
- Some variants require `rtc_gpio_hold_dis()` on reset pin after deep sleep

---

## References

### Upstream Libraries

| Library     | Purpose                               | Link                              |
| ----------- | ------------------------------------- | --------------------------------- |
| **GxEPD2**  | SPI e-paper driver for Arduino        | https://github.com/ZinggJM/GxEPD2 |
| **FastEPD** | Parallel e-paper driver (T5 4.7 inch) | Bundled with T5 S3 examples       |

### LilyGO Resources

| Resource             | URL                                                |
| -------------------- | -------------------------------------------------- |
| LilyGO GitHub        | https://github.com/Xinyuan-LilyGO                  |
| T5 S3 Pro repository | https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper |
| T-Echo repository    | https://github.com/Xinyuan-LilyGO/LilyGo-T-Echo    |

### Meshtastic Documentation

| Document                                                                  | Purpose                      |
| ------------------------------------------------------------------------- | ---------------------------- |
| [epaper-display-subsystem.md](epaper-display-subsystem.md)                | General e-paper architecture |
| [Hardware Selection Guide](https://meshtastic.org/docs/hardware/devices/) | Official supported hardware  |

---

## Revision History

| Date    | Changes                         |
| ------- | ------------------------------- |
| 2025-01 | Initial comprehensive reference |

---

_For questions or corrections to this document, refer to the Meshtastic firmware repository and community forums._
