# T5S3 E-Paper Pro Development Log

## Overview

Efforts today focused on stabilizing the LilyGo T5S3 E-Paper Pro (H752-01) variant, specifically addressing UI visibility (clipping), button mapping conflicts, and reliable backlight (frontlight) control.

## 🛠 Key Changes & Attempts

### 1. Screen Clipping (Bezel Compensation)

- **Problem:** The physical bezel covers the outer edges of the 960x540 panel, hiding status icons and text.
- **Fix:** Added `#define EPD_PADDING 24` to `variant.h`.
- **Implementation:** Modified `EInkParallelDisplay` to reduce the logical `BaseUI` resolution to 912x492 and apply a centering offset during every `display()` call.
- **Status:** Verified functional; content is now visible.

### 2. Button Mapping & I2C Driver

- **Problem:** The device has 5-6 buttons, but only BOOT (GPIO 0) was functional. Attempts to use IO48 as a direct GPIO caused floating-pin interference (rapid screen swapping).
- **Findings:** The T5S3 Pro V2 uses a **PCA9535 I2C Expander**. PWR and the side buttons are wired to Port 1 (P1.2, P1.3, etc.).
- **Implementation:**
  - Created `src/input/PCA9535ButtonThread` to poll the expander every 20ms.
  - Implemented a 150ms software debounce to stop "contact bounce" from triggering multiple events.
  - Mapped **Side Button 1** (P1.3/IO48) to a new `INPUT_BROKER_BACKLIGHT_TOGGLE` event.
  - Mapped **PWR Button** (P1.2) to `INPUT_BROKER_ALT_PRESS` (Back).
- **Status:** I2C driver is functional but logic conflicts remain.

### 3. Backlight Control Synchronization

- **Problem:** Pressing the BOOT button to navigate would wake the screen and force the backlight ON, overriding the user's manual "OFF" toggle.
- **Fix:** Added `bool backlightOnPreference` to the `Screen` class.
- **Implementation:** Updated `handleSetOn()` to respect the preference during wake events.
- **Status:** **UNSTABLE.** The toggle is still unreliable, often requiring combined button presses or failing to stay in the requested state.

## 🔴 Current Blockers & Issues

1. **Backlight Conflict:** The "Wake on Press" logic in the core firmware continues to fight with the manual backlight toggle on this specific hardware.
2. **Device Speed:** The E-Ink refresh is slow enough that the input broker sometimes misses or double-processes events during a screen update.
3. **Flashing Issues:** Frequent `Errno 71 (Protocol Error)` during uploads require manual resets into Download Mode.

## 📋 Next Steps

- Completely refactor the `PowerFSM` interaction with `BOARD_BL_EN` for E-Ink variants.
- Investigate if `FastEPD` is internally resetting the I2C expander pins during its own initialization.
- Move the `backlightOnPreference` into persistent NVS storage so it survives reboots.

---

## Phase 2: Chapter Cache & Progress Persistence (May 9–11, 2026)

**Branch:** `t5s3-epaper-ereader-poc`

### What was done

Study of CrossPoint Reader (MIT, https://github.com/song9466/CrossPoint-Reader) revealed two discrete systems to replicate:

1. **Chapter cache** — decompress sections on-demand, store raw text in `/sd/.crosspoint/epub_<8-hex>/sections/<idx>.txt`. Avoids re-parsing the ZIP and re-decompressing HTML on every open.
2. **Progress persistence** — save `chapterIndex`, `pageOffset`, `timestamp` as a 12-byte binary to `progress.bin` alongside the cache.

### Implementation

- `src/reader/ChapterCache.h/cpp` — FNV-1a 32-bit hash for deterministic cache keys (no `std::hash` variance), `ensureCacheRootForEpub()`, `sectionPath()`, `progressPath()`, `bookCachePath()`
- `src/reader/ProgressStore.h/cpp` — `ReadingProgress` struct (12 bytes), `loadReadingProgress()`, `saveReadingProgress()`
- `src/reader/EpubParser.h/cpp` — cache-aware `getChapter()` (cache-first, ZIP fallback, write-through on miss), `isSdCardPresent()`, `getBookPath()`, `invalidateChapterCache()`
- `src/reader/ReaderFSM.h/cpp` — `openBookPath`, `loadProgress()`, `saveProgress()` with 2 s debounce; progress auto-saved on book close and every 2 s during reading; restored on book open

### Preserved (another agent's pre-existing changes)

- VFS `opendir`/`readdir` book scan in `ReaderFSM.cpp`
- SD retry-at-4MHz on failure in `FSCommon.cpp`

### Tests

- `test/test_reader/test_fnv_hash.cpp` — FNV-1a known-vector verification (4 cases)
- `test/test_reader/test_progress_store.cpp` — 12-byte struct serialization round-trip (4 cases)

### Build

- `pio run -e t5s3-epaper-v2-reader` — ✅ exit 0

### Blockers

- Native unit tests (`pio test -e native`) blocked by missing `libyaml-cpp-dev` on host (no sudo available)
