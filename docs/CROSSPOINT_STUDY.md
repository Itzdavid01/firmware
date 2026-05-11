# CrossPoint Study — T5S3 E-Reader Enhancement

**Studying**: [crosspoint-reader/crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) (`20fee843c721e5392c1854bee250538153d92860`)
**License**: MIT (same as source if we reference any patterns)
**Hardware context**: ESP32-C3 / 380KB RAM (vs. our ESP32-S3 / 8MB PSRAM)
**Why**: CrossPoint is the only mature open-source e-reader targeting constrained ESP32 hardware; its SD caching architecture is directly relevant to our chapter-cache and progress-saving goals.

---

## 1. Summary

CrossPoint Reader is a dedicated e-reader firmware for the ESP32-C3 with ~380KB usable RAM, written in C++ with a custom HAL (`open-x4-sdk`). It implements EPUB parsing, a binary SD-card chapter cache (`.crosspoint/` directory), WebSocket+WebDAV Calibre integration, WiFi OTA, and deep-sleep power management. The architecture is shaped entirely by the 380KB RAM ceiling — every design decision optimises for minimal RAM usage with SD-backed storage for all large or slowly-built data. Our T5S3 has 8MB PSRAM — we are orders of magnitude less constrained — so we can adopt the _pattern_ (SD chapter cache) without the _desperation_ (extreme byte-packing).

---

## 2. License & Attribution

- **License**: MIT (per repo README)
- **Files we may reference** (file:line citations from this study):

| CrossPoint file                                | Purpose                                            | Our relevant use                      |
| ---------------------------------------------- | -------------------------------------------------- | ------------------------------------- |
| `lib/Epub/Epub.h` / `Epub.cpp`                 | EPUB open/parse + cache path construction          | Cache key strategy                    |
| `lib/Epub/Epub/BookMetadataCache.h`            | Spine+ToC binary cache (`book.bin`)                | Binary cache format design            |
| `lib/Epub/Epub/Section.h` / `Section.cpp`      | Chapter section file load/create                   | Pre-layout binary page storage        |
| `src/activities/reader/EpubReaderActivity.cpp` | `silentIndexNextChapterIfNeeded()` prefetch        | Chapter prefetch hinting              |
| `src/network/WebDAVHandler.cpp`                | WebDAV PUT handler + cache invalidation            | SD file operations                    |
| `src/network/CrossPointWebServer.cpp`          | WebSocket upload `START`/`READY`/`DONE`            | Protocol reference (not implementing) |
| `src/main.cpp`                                 | Sleep timeout / `lastActivityTime` reset           | Sleep integration pattern             |
| `lib/hal/HalPowerManager.cpp`                  | CPU freq scaling + deep sleep                      | Power integration pattern             |
| `docs/file-formats.md`                         | Binary format layouts for `book.bin`/`section.bin` | Cache file format reference           |

**Attribution line** for any file where we port a CrossPoint pattern:

```cpp
// Cache strategy inspired by CrossPoint Reader (MIT).
// https://github.com/crosspoint-reader/crosspoint-reader
```

---

## 3. Architecture Overview

```
T5S3 E-Reader (ESP32-S3 / 8MB PSRAM / 16MB flash / SX1262 LoRa)
│
├─ Meshtastic firmware (Router, NodeDB, CryptoEngine, PowerFSM)
│   └─ ReaderFSM ← top-level reader state machine (BOOK_LIST | READING | SETTINGS | SUSPENDED)
│       ├─ EpubParser    ← parses .epub spine order; miniz for ZIP extraction
│       ├─ DocumentRenderer  ← HTML strip + drawStringMaxWidth auto-wrap
│       └─ Screen        ← FastEPD + ED047TC1Parallel display
│
└─ SD card (FAT32, mounted at /sd by main firmware)
    │
    └── .crosspoint/                    ← (NEW) CrossPoint-inspired cache root
        └── epub_<hash>/                 ← one dir per book, keyed by path-hash
            ├── book.bin                 ← (NEW) spine+TOC binary cache
            ├── progress.bin             ← (NEW) reading position
            └── sections/               ← (NEW) pre-computed page binary per chapter
                ├── 0.bin
                └── 1.bin
```

### CrossPoint Task Structure (reference)

CrossPoint runs two tasks:

- **Main task** (Arduino loop, priority 1): input polling + `activityManager.loop()` + sleep timeout watchdog
- **Render task** (priority 1, 8KB stack): blocked by `xTaskNotify()`; acquires `renderingMutex`; calls `activity->render()`

We do **not** need this — our reader runs synchronously inside the existing Meshtastic event loop. The `ReaderFSM::handleInputEvent()` is already called from the InputBroker callback.

---

## 4. Subsystem Deep-Dives

### 4a. EPUB Parser & Spine Loading

**How it works** (CrossPoint `lib/Epub/Epub.cpp` + `BookMetadataCache`):

1. `Epub::load()` opens `.epub` (ZIP via miniz-equivalent), reads `META-INF/container.xml` → finds `.opf`
2. `parseContentOpf()` builds spine manifest + TOC + CSS references in streaming fashion
3. All spine/TOC data written directly to `book.bin` on SD (never buffered in RAM)
4. `Section::loadSectionFile()` checks SD for `sections/<n>.bin` first — hits cache if layout params match
5. If cache miss: parse HTML → layout pages → write `sections/<n>.bin` → return pages from RAM

**Why it works that way**: 380KB RAM can't hold a full chapter's DOM + layout. CrossPoint streams to SD during the parse, then reads back the binary cache in small chunks per page render.

**Ports to our ESP32-S3?** Yes — cleanly. We have 8MB PSRAM so we can hold more in RAM, but the SD cache pattern is still valuable for:

- Large chapters (1MB+ HTML files that would OOM on extract-to-heap)
- Fast cold-start page turns (pre-computed pages in `.bin` vs. re-parsing HTML each time)
- Progress persistence across reboots

**Specific risks**: None. The cache-key-by-path-hash pattern and LUT-based binary format are generic.

**Incompatibility**: CrossPoint uses `open-x4-sdk`'s `Storage` abstraction. We'll use the Meshtastic `SD` namespace (already available via `<SD.h>`) and write raw binary files.

---

### 4b. SD Card Cache Implementation (`.crosspoint/`)

**How it works** (CrossPoint `lib/Epub/Epub.h:47`):

```
Cache root: /.crosspoint/epub_<hash>/
  ├── book.bin         spine+TOC+metadata, built once per book
  ├── progress.bin     chapter index + page offset + timestamp
  ├── cover.bmp       generated cover thumbnail
  └── sections/
      ├── 0.bin        page binary for spine item 0
      └── 1.bin        page binary for spine item 1
```

**Cache key** (`Epub.h:36`): `std::hash<std::string>{}(filepath)` — a numeric hash of the full EPUB path.

**Cache invalidation** (`Epub.cpp`): When layout settings (font, line spacing, viewport) change, `Storage.removeDir(cachePath + "/sections")` deletes the sections directory. `book.bin` is only rebuilt if the EPUB itself changes.

**In our code**: `ReaderFSM` already has `currentChapter` and `currentPageOffset`. We need:

1. A `ChapterCache` class that manages `/.crosspoint/epub_<hash>/` paths
2. Write `progress.bin` on every page turn (debounced — not every single input event)
3. Load `progress.bin` on book open and restore `currentChapter`/`currentPageOffset`

**Ports cleanly?** Yes. The hard constraint: Meshtastic's `SD` card must already be initialised and mounted. The reader currently calls `opendir("/sd")` without checking `SD.begin()` status — that is the first thing to fix.

---

### 4c. Memory Model & Chunking

**CrossPoint constraint** (per README):

> The ESP32-C3 only has ~380KB of usable RAM. A lot of the decisions were based on this constraint.

**Consequences in CrossPoint**:

- CSS parsed on-demand with 64KB heap minimum check before allocating
- `readItemContentsToStream()` uses configurable 512–1024 byte chunks from ZIP, not `mz_zip_reader_extract_file_to_heap`
- HTML parsed to page-stream then discarded; `Section` holds only pre-computed `PageLine` arrays
- One chapter in RAM at a time (`EpubReaderActivity` holds exactly one `Section`)

**Our situation**: ESP32-S3 with 8MB PSRAM. `miniz` `mz_zip_reader_extract_file_to_heap` for chapter HTML is acceptable for chapters up to ~500KB. For larger chapters (1MB+ HTML), we risk OOM.

**Recommendation**: For Phase 2, implement **chunked ZIP extraction** for `getChapter()` — read the chapter file in streaming chunks instead of extracting the whole file to heap. The `miniz` API supports this via `mz_zip_reader_read_raw_data` / `mz_zip_reader_read_data` callbacks. However, since our immediate goal is the chapter cache (not fixing OOM), the cache itself acts as the memory relief: we only need to hold one chapter's content at a time.

---

### 4d. Calibre Integration (WiFi + WebDAV)

**CrossPoint protocol**:

- Device runs HTTP (port 80) + WebSocket (port 81) + mDNS (`crosspoint.local`)
- Calibre plugin discovers device via UDP broadcast to ports 8134, 54982, etc.
- Plugin sends: `START:<filename>:<size>:<path>` → `READY` → binary chunks → `DONE`
- Alternative WebDAV: `PUT /*.epub` with streaming upload

**Does NOT port cleanly to our context**: Meshtastic is a mesh radio that operates independently of WiFi. Adding a WiFi access point + WebSocket server + Calibre plugin is:

- Architecturally entangled with Meshtastic's WiFi stack (which may be excluded on this build)
- Outside the Phase 2 scope (SD chapter cache only)
- A future enhancement for when the reader is a standalone foreground app

**Recommendation**: **Do not adopt in Phase 2.** File a separate issue for "Add WiFi AP + Calibre plugin for book transfer" as a later phase.

---

### 4e. OTA Update Mechanism

**CrossPoint paths**:

1. **WiFi OTA**: Query `api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/latest`, streaming JSON parse via SDK, `esp_https_ota_begin/perform/finish` into inactive OTA partition
2. **SD-card flash**: `validateImageFile()` (header magic + segment walk + XOR checksum + optional SHA256), raw 64KB block erase+write, direct otadata partition write via `OtaBootSwitch`

**Why the otadata bypass**: ESP-IDF's `esp_image_verify` rejects patched firmware on X4 silicon. CrossPoint's bootloader accepts it; the runtime does not. Both paths bypass `esp_ota_set_boot_partition()`.

**Does NOT port**: Meshtastic has its own OTA pipeline (`meshtastic_update_flash` MCP tool + `bin/device-update.sh`). We should not reimplement this. The reader module has no business driving firmware OTA.

**Recommendation**: **Do not adopt.** Rely on Meshtastic's existing OTA flow.

---

### 4f. Font System

**CrossPoint approach**:

- Built-in: NotoSans, NotoSerif, OpenDyslexic as `const uint8_t[]` compiled into firmware
- SD card: `.cpfont` v4 binary format — deflate-compressed, interval-indexed glyphs, per-page mini kern matrix (~625 bytes for Latin page vs. 36KB full matrix)
- One size per font family resident in RAM at a time; on-demand glyph decompression via `FontDecompressor`

**Our situation**: We use AdafruitGFX fonts (`ArialMT_Plain_16`, etc.) via the FastEPD/GxEPD2 stack. Font loading is handled by the display driver, not by the reader.

**Recommendation**: **Do not adopt.** Our existing font stack is sufficient for Phase 2. The reader uses `display->setFont()` and `display->drawStringMaxWidth()`. If we need better fonts later, that is a separate effort.

---

### 4g. Power Management / Sleep

**CrossPoint**:

- CPU frequency scaling: 10 MHz idle / normal MHz on activity (via `HalPowerManager::Lock` RAII)
- Deep sleep: `GPIO_SPIWP` (GPIO13) cut battery-latch MOSFET, `esp_deep_sleep_start()` with GPIO wake on power button
- Sleep timeout: `lastActivityTime` reset on any button press/release; `enterDeepSleep()` called when `millis() - lastActivityTime >= sleepTimeoutMs`

**Our situation**: Meshtastic has `PowerFSM` with `stateON / stateDARK / stateSERIAL / statePOWER`. The T5S3 variant has `SLEEP_TIME 120` and `USE_POWERSAVE`. The backlight toggle is currently unstable per `T5S3_DEVELOPMENT_LOG.md`.

**Recommendation**: **Adopt later.** The power management is entangled with the broader Meshtastic PowerFSM. For Phase 2, the reader should participate in the existing power state by calling `screen->setUseDisplay()` appropriately. Direct power state changes are out of scope.

---

### 4h. Input Handling

**CrossPoint** (`InputManager` → `HalGPIO` → `MappedInputManager` → `ButtonNavigator`):

- Physical buttons debounced via SDK's `InputManager`
- User remapping via `MappedInputManager` (front button remap + side button layout)
- `ButtonNavigator`: press callback + release callback + continuous callback (held > `continuousStartMs`, fires every `continuousIntervalMs`)
- Page navigation helpers (`nextPageIndex`, `previousPageIndex`)

**Our situation**: `ReaderFSM::handleInputEvent()` receives `InputEvent*` from `InputBroker`. Button debouncing is handled by `PCA9535ButtonThread` (per `T5S3_DEVELOPMENT_LOG.md`). The reader's navigation uses simple `INPUT_BROKER_UP/DOWN/SELECT/BACK` events.

**Recommendation**: **Adopt the continuous-callback pattern for page turns.** Instead of one event per button press, use a timer-based approach where holding DOWN repeatedly triggers page turns. This matches the existing "long press" detection pattern in Meshtastic but is currently not used for reading navigation. The reader's `INPUT_BROKER_USER_PRESS` is already mapped to "next page" — this is fine for Phase 2.

---

### 4i. Partition Table & Flash Layout

**CrossPoint** (`partitions.csv`): 16MB flash, dual OTA (`app0` 6.25MB, `app1` 6.25MB), `spiffs` 3.375MB, `otadata` 8KB.

**Our T5S3** (`variants/esp32s3/t5s3_epaper/platformio.ini`): `board_build.partition = default_16MB.csv` — same layout.

**OTA note**: Meshtastic uses a single-firmware layout on many targets; OTA is handled via `meshtastic_update_flash` (MCP) or `bin/device-update.sh`. No changes needed to partition table for Phase 2.

---

## 5. Comparison Matrix

| Aspect             | CrossPoint Reader                                          | Our T5S3 Reader (current)                                               | Verdict                                                   |
| ------------------ | ---------------------------------------------------------- | ----------------------------------------------------------------------- | --------------------------------------------------------- |
| **EPUB parsing**   | Spine + ToC parsing, streaming to `book.bin` on SD         | Spine parsing works (✓), ToC not yet implemented                        | We lead on spine order; CrossPoint leads on SD-backed TOC |
| **Memory model**   | SD-first; RAM for one section only                         | Full chapter extracted to heap via `mz_zip_reader_extract_file_to_heap` | CrossPoint wins for large chapters; ours wins for small   |
| **SD usage**       | `.crosspoint/` cache dir, binary `section.bin` per chapter | `/sd` VFS scan only; no caching                                         | CrossPoint wins significantly                             |
| **Font system**    | `.cpfont` v4 with deflate + interval indexing              | AdafruitGFX built-in fonts via display driver                           | Draw — different stack, not directly comparable           |
| **Input handling** | Continuous-callback `ButtonNavigator`                      | Simple `InputBroker` event per press                                    | CrossPoint pattern is better for reading nav              |
| **OTA**            | Dual-OTA + GitHub API checker + SD flash                   | Meshtastic MCP OTA + `device-update.sh`                                 | Meshtastic wins — existing infrastructure                 |
| **WiFi/Calibre**   | WebSocket + WebDAV + mDNS discovery                        | None                                                                    | Out of scope for Phase 2                                  |
| **Power model**    | Deep sleep via battery latch + GPIO wake                   | Meshtastic `PowerFSM` + `SLEEP_TIME`                                    | Entangled; defer to later phase                           |
| **Chapter cache**  | Binary `section.bin` with page LUT                         | None                                                                    | CrossPoint is the reference for Phase 2                   |

---

## 6. Recommended Adoptions, Ranked

### Adopt Now (Phase 2 scope)

1. **SD card detection fix** — Verify `SD.begin()` succeeds before scanning; log card type/size; error gracefully if no card present. This is prerequisite to everything else.
2. **Progress saving** (`progress.bin`) — Write `{chapterIndex, pageOffset, timestamp}` to `/.crosspoint/epub_<hash>/progress.bin` on page turns (debounced to every 2s or every chapter change). Load on book open. Survives reboots.
3. **Chapter cache directory** — Create `/.crosspoint/epub_<hash>/sections/` on first open of a book. Store cache key derivation in a `ChapterCache` class.
4. **Cache-aware `getChapter()`** — Before re-extracting from ZIP, check if a cached chapter file exists. If yes, read from cache. If no, extract and write the cache file.

### Adopt Later

5. **Smart pagination** — `DocumentRenderer::calculatePages()` using measured character widths from `display->getStringWidth()`. Already planned in `T5S3_EREADER_ROADMAP.md` (item 3). CrossPoint's `Section::loadSectionFile()` + `PageLine` binary format is the reference.
6. **Prefetch next chapter** — In `ReaderFSM`, when `currentPageOffset > (chapterLength - 1 page)`, pre-open the next chapter's cache file in a background read. CrossPoint's `silentIndexNextChapterIfNeeded()` is the pattern.
7. **Continuous page-turn** — Replace single "next page" event with a `ButtonNavigator`-style continuous callback that fires page turns every 300ms while held.
8. **Chapter cache invalidation** — When font/margin settings change, delete `sections/` directory. CrossPoint's `Storage.removeDir()` pattern.

### Do Not Adopt

9. **WiFi AP + Calibre integration** — Architecturally wrong fit; out of scope for SD-cache phase.
10. **OTA reimplementation** — Use Meshtastic's existing MCP OTA flow.
11. **`.cpfont` v4 font format** — Different font stack; AdafruitGFX is sufficient.
12. **Deep sleep reimplementation** — Entangled with `PowerFSM`; wait for dedicated power-management phase.
13. **KOReader XPath progress mapping** — Useful only for KOReader interop; out of scope.

---

## 7. Open Questions

| Question                                                                                         | How to Answer                                                                                               |
| ------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------- |
| Is `SD.begin()` in `setupSDCard()` called before or after `ReaderFSM::init()`?                   | Check `src/main.cpp` boot order (lines ~815+) and `ReaderFSM::init()` call site                             |
| Does the SD card stay mounted when the reader is active, or does Meshtastic unmount it on sleep? | Add `SD.cardType()` polling to `ReaderFSM::scanForBooks()` and log the result                               |
| What is the maximum safe heap for a single chapter extraction?                                   | Run `ESP.getFreeHeap()` before/after `mz_zip_reader_extract_file_to_heap` for a large EPUB; set a threshold |
| Does the existing `PowerFSM` interfere with backlight toggle?                                    | Add debug logs to `Screen::handleSetOn()` in `src/graphics/Screen.cpp`                                      |
| Are there existing unit tests for the reader components?                                         | Search `test/` for reader-related tests (likely none yet)                                                   |

---

_Study completed: May 2026_
_CrossPoint commit: `20fee843c721e5392c1854bee250538153d92860`_
