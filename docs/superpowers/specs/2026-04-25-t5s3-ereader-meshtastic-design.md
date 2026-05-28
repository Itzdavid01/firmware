# T5 S3 E-Paper Pro — E-Reader + Meshtastic Design

**Branch:** `t5s3-epaper-ereader-poc`
**Target hardware:** LILYGO T5 E-Paper S3 Pro H752-02 (V2 — GPS + SD on SPI1)
**Date:** 2026-04-25

---

## Problem Statement

The existing `t5s3_epaper_inkhud` env is marked `board_level = extra` (incomplete) because:

1. `nicheGraphics.h` was copy-pasted from Heltec VM-E290 — wrong display driver (`DEPG0290BNS800`, SPI) for a device that uses a 960×540 ED047TC1 over 8-bit parallel bus
2. No InkHUD-compatible parallel display driver exists in `src/graphics/niche/Drivers/EInk/`

Goal: fix the InkHUD port AND extend the device with a standalone e-reader (EPUB, PDF, TXT, Markdown) that coexists with the full Meshtastic mesh stack.

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  T5 S3 E-Paper Pro                  │
│                                                     │
│  ┌──────────────┐        ┌───────────────────────┐  │
│  │  ReaderFSM   │◄──────►│       InkHUD          │  │
│  │  (doc render)│        │  (mesh UI, applets)   │  │
│  └──────┬───────┘        └──────────┬────────────┘  │
│         │                           │               │
│         └──────────┬────────────────┘               │
│                    │                                │
│         ┌──────────▼────────────┐                   │
│         │  DisplayOwner (flag)  │                   │
│         └──────────┬────────────┘                   │
│                    │                                │
│         ┌──────────▼────────────┐                   │
│         │  ED047TC1Parallel     │                   │
│         │  (InkHUD driver)      │                   │
│         └───────────────────────┘                   │
│                                                     │
│  SD Card: /books/             Mesh: always running  │
│  PSRAM: page buffer           LoRa: background      │
└─────────────────────────────────────────────────────┘
```

Two subsystems share one display via a cooperative ownership flag. The mesh stack (LoRa, routing, modules) runs continuously in both modes — only the UI ownership switches.

---

## Section 1: Parallel Display Driver

**Problem:** All InkHUD drivers in `src/graphics/niche/Drivers/EInk/` are SPI-based. The T5 S3 E-Paper Pro uses FastEPD over an 8-bit parallel bus.

**Solution:** Adapter driver that wraps FastEPD behind the standard `Drivers::EInk` interface InkHUD expects.

**New file:** `src/graphics/niche/Drivers/EInk/ED047TC1Parallel.h/cpp`

Responsibilities:

- Inherit from `Drivers::EInk`
- Translate InkHUD's 1-bit GFX framebuffer → FastEPD's parallel framebuffer format
- Map `setDisplayResilience()` fast/full refresh policy → FastEPD update modes
- Handle V1/V2 pin differences via compile-time flags already in `variant.h`

**`nicheGraphics.h` fix:**

- Remove all Heltec VM-E290 content and wrong comment
- Instantiate `ED047TC1Parallel` instead of `DEPG0290BNS800`
- Wire GT911 touch via SensorLib
- Two-button setup for V2 (BUTTON_PIN 0 only)
- Call `ReaderFSM::init()` alongside `inkhud->begin()`
- Set `maxCount = 2` tiles (960×540 has room)

---

## Section 2: ReaderFSM

**Location:** `src/reader/ReaderFSM.cpp/.h`

Subclasses `OSThread` — fits existing periodic-work pattern. Registers with `InputBroker` for touch events and button presses.

**States:**

```
IDLE ──(open book)──► READING ──(long press)──► [yield to InkHUD]
  ▲                      │
  └──(close book)────────┘

READING ──(mesh message)──► BANNER(4s) ──(auto/touch)──► READING
```

**Document rendering stack:**

| Format         | Parser                                           | Renderer           |
| -------------- | ------------------------------------------------ | ------------------ |
| TXT / Markdown | Direct                                           | Text layout engine |
| EPUB           | miniz (ZIP) → HTML tag stripper → text extractor | Text layout engine |
| PDF            | Minimal PDF object parser (text streams only)    | Text layout engine |

Single text layout engine for all formats: word wrap, configurable font size, line spacing, targeting 960×540. Pages pre-chunked on book open — store character-offset index in PSRAM (not full bitmaps). Actual rendering on page turn → PSRAM framebuffer → display flush.

Complex PDFs degrade gracefully to text-only extraction.

**Libraries:**

- ZIP: `miniz` (~50KB flash, used widely in ESP32 ecosystem)
- No external HTML renderer — strip tags, extract text, reflow
- PDF: custom minimal parser or `updf`

**SD card layout:**

```
/books/
  my-book.epub
  document.pdf
  notes.txt
/bookmarks/
  my-book.json    ← last page position + bookmarks per book
```

---

## Section 3: Display Ownership Protocol

**Flag:**

```cpp
// src/reader/DisplayOwner.h
enum class DisplayOwner { INKHUD, READER };
extern DisplayOwner activeDisplayOwner; // defined in ReaderFSM.cpp, default INKHUD
```

Cooperative handoff — no mutex needed (both FSMs on main loop thread, no preemption). Yielding side finishes current frame, sets flag. Acquiring side takes over next tick.

**Mode switch (long press on BUTTON_PIN 0):**

| Direction       | Sequence                                                                 |
| --------------- | ------------------------------------------------------------------------ |
| READER → INKHUD | Save page position → set flag INKHUD → InkHUD resumes last applet        |
| INKHUD → READER | InkHUD finishes frame → set flag READER → ReaderFSM redraws current page |

**Mesh banner:**

When `activeDisplayOwner == READER` and a mesh message arrives:

1. `ReaderFSM` observes `MeshModule` message observable (existing Observer pattern)
2. Transition to `BANNER` state — slim overlay bar at top of current page (sender + message, 60 char max)
3. Auto-return to `READING` after 4 seconds, or on touch
4. Long press during banner still triggers full mode switch to InkHUD

**Touch zones in reader mode:**

| Zone       | Action                                 |
| ---------- | -------------------------------------- |
| Left 30%   | Previous page                          |
| Right 30%  | Next page                              |
| Center tap | Book menu (title, chapters, bookmarks) |

---

## Section 4: Variant Config

**New env:** `t5s3-epaper-v2-reader`

```ini
[env:t5s3-epaper-v2-reader]
extends = t5s3_epaper_base, inkhud
build_flags =
  ${t5s3_epaper_base.build_flags}
  ${inkhud.build_flags}
  -D T5_S3_EPAPER_PRO_V2
  -D SDCARD_USE_SPI1
  -D GPS_POWER_TOGGLE
  -D MESHTASTIC_INCLUDE_READER
build_src_filter =
  ${t5s3_epaper_base.build_src_filter}
  ${inkhud.build_src_filter}
  +<../../../src/reader>
lib_deps =
  ${inkhud.lib_deps}
  ${t5s3_epaper_base.lib_deps}
  # miniz for EPUB/ZIP parsing
  <miniz pin TBD>
```

`board_level` not set — this is the primary target env, not `extra`.

**Directory layout:**

```
src/reader/
  ReaderFSM.cpp / .h
  DisplayOwner.h
  DocumentRenderer.cpp / .h
  formats/
    EpubParser.cpp / .h
    PdfParser.cpp / .h
    TxtParser.cpp / .h
src/graphics/niche/Drivers/EInk/
  ED047TC1Parallel.cpp / .h     ← new
variants/esp32s3/t5s3_epaper/
  nicheGraphics.h               ← rewritten
  platformio.ini                ← new env added
```

---

## Open Questions / Risks

- **H752-02 revision:** Not yet in firmware. Pin differences vs H752-01 unknown until device arrives. May need a V3 define.
- **miniz pin:** Exact commit/tag TBD — confirm no flash size conflict with InkHUD + reader stack.
- **PDF complexity:** Text-only extraction is realistic; complex layouts (multi-column, embedded images) will not render correctly. Acceptable degradation for a v1.
- **PSRAM availability:** Page-offset index approach assumes PSRAM present. Verify board spec before implementation.
- **FastEPD ↔ InkHUD framebuffer format:** Bit depth and byte order alignment between the two stacks needs verification during driver implementation.
