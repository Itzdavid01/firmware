# T5S3 CrossPoint-Inspired Implementation Plan — Phase 2

**Based on**: `docs/CROSSPOINT_STUDY.md`
**Scope**: SD-card-backed chapter cache + progress saving for the existing EPUB pipeline, plus prerequisite SD card detection fix.
**Out of scope**: WiFi/Calibre, OTA, font changes, power state changes.

---

## 1. Scope Statement

### In Scope (Phase 2)

- **SD card detection fix**: Verify `SD.begin()` succeeds; log card type/size; fail gracefully with a clear message if card is missing or unreadable
- **Chapter cache**: `ChapterCache` class that manages `/.crosspoint/epub_<hash>/sections/<n>.bin` binary chapter files
- **Progress saving**: `progress.bin` with `{chapterIndex, pageOffset, timestamp}` — written debounced, loaded on book open
- **Cache-aware `EpubParser::getChapter()`**: Check cache first → hit → read from SD; miss → extract from ZIP + write cache
- **Cache key class**: `CacheKey` — deterministic hash of EPUB filepath + layout settings (font/margins not in Phase 1, so just filepath hash for now)

### Out of Scope (Later Phases)

- Smart pagination (`DocumentRenderer::calculatePages()`) — tracked in `T5S3_EREADER_ROADMAP.md` item 3
- Chapter prefetch (next-chapter pre-loading)
- Continuous page-turn input
- Font/settings-based cache invalidation
- WiFi AP + Calibre integration
- OTA changes
- Deep sleep / power state changes

---

## 2. Interfaces & Types

### New File: `src/reader/ChapterCache.h`

```cpp
#pragma once
#include <string>
#include <cstdint>

namespace reader
{

// Cache key: deterministic hash of book filepath.
class CacheKey
{
  public:
    // e.g. "/sd/.crosspoint/epub_12345678/"
    static std::string cacheRootForEpub(const std::string& epubPath);

    // e.g. "/sd/.crosspoint/epub_12345678/sections/0.bin"
    static std::string sectionPath(const std::string& epubPath, int chapterIndex);

    // e.g. "/sd/.crosspoint/epub_12345678/progress.bin"
    static std::string progressPath(const std::string& epubPath);

    // e.g. "/sd/.crosspoint/epub_12345678/book.bin"
    static std::string bookCachePath(const std::string& epubPath);
};

} // namespace reader
```

### New File: `src/reader/ChapterCache.cpp`

- `CacheKey::cacheRootForEpub()`: compute `std::hash<std::string>{}(epubPath)` → `"/sd/.crosspoint/epub_<hash>"`; create the directory via `SD.mkdir()`
- `CacheKey::sectionPath()`: return `cacheRootForEpub() + "/sections/<n>.bin"`
- `CacheKey::progressPath()`: return `cacheRootForEpub() + "/progress.bin"`
- `CacheKey::bookCachePath()`: return `cacheRootForEpub() + "/book.bin"`

### Modified File: `src/reader/EpubParser.h`

```cpp
class EpubParser
{
  public:
    // ... existing signatures ...

    std::string getChapter(int index) override;

    void invalidateChapterCache();

    bool isSdCardPresent() const;
};
```

### New File: `src/reader/ProgressStore.h`

```cpp
#pragma once
#include <cstdint>
#include <string>

namespace reader
{

struct ReadingProgress
{
    int chapterIndex = 0;
    int pageOffset = 0;
    uint32_t timestamp = 0;
};

bool loadProgress(const std::string& epubPath, ReadingProgress& out);
bool saveProgress(const std::string& epubPath, const ReadingProgress& progress);

} // namespace reader
```

### Modified File: `src/reader/ReaderFSM.h`

```cpp
class ReaderFSM
{
  public:
    // ... existing signatures ...

    void saveProgress();
    void loadProgress();

  private:
    // ... existing fields ...

    uint32_t lastProgressSaveMs = 0;
    static constexpr uint32_t PROGRESS_SAVE_DEBOUNCE_MS = 2000;
};
```

### Modified File: `src/reader/ReaderFSM.cpp`

- On `READING` state entry: call `loadProgress()` → restore `currentChapter`/`currentPageOffset`
- On every page turn: call `saveProgress()` if debounce interval elapsed
- On book list selection (entering READING): call `parser.invalidateChapterCacheIfNeeded()` when settings change

---

## 3. File-by-File Change List

| File                           | Action     | Reason                                                                                           |
| ------------------------------ | ---------- | ------------------------------------------------------------------------------------------------ |
| `src/reader/ChapterCache.h`    | **CREATE** | Cache key derivation + path helpers                                                              |
| `src/reader/ChapterCache.cpp`  | **CREATE** | `CacheKey` implementation + SD dir creation                                                      |
| `src/reader/ProgressStore.h`   | **CREATE** | `ReadingProgress` struct + load/save declarations                                                |
| `src/reader/ProgressStore.cpp` | **CREATE** | Binary progress file I/O (struct serialization)                                                  |
| `src/reader/EpubParser.h`      | **MODIFY** | Add `getChapter(int)` cache-aware override, `invalidateChapterCache()`                           |
| `src/reader/EpubParser.cpp`    | **MODIFY** | Check SD cache before ZIP extract; write cache on miss                                           |
| `src/reader/ReaderFSM.h`       | **MODIFY** | Add `saveProgress()`, `loadProgress()`, debounce field                                           |
| `src/reader/ReaderFSM.cpp`     | **MODIFY** | Load progress on book open; save progress on page turns (debounced)                              |
| `test/test_reader/`            | **CREATE** | Native unit tests: `CacheKey` hash uniqueness, `ProgressStore` round-trip, cache path derivation |
| `docs/T5S3_EREADER_ROADMAP.md` | **MODIFY** | Mark items 3 (smart pagination) and 4 (progress saving) as in-progress or done                   |
| `docs/T5S3_DEVELOPMENT_LOG.md` | **MODIFY** | Add Phase 2 entry at top                                                                         |

### Files NOT modified (staying untouched)

- `src/mesh/`, `src/modules/` (core Meshtastic)
- `src/PowerFSM.*`, `src/graphics/Screen.cpp` (power/backlight)
- `src/input/PCA9535ButtonThread*` (button driver)
- `src/graphics/niche/**` (InkHUD — not relevant to reader module)

---

## 4. Test Plan

### Native Unit Tests (`pio test -e native`)

Create `test/test_reader/` with:

1. **`test_cache_key`** — verify `CacheKey::cacheRootForEpub()`:
   - Same path → same hash
   - Different paths → different hashes (collision resistance spot-check)
   - Directory creation is not testable in native (no SD), but path generation is

2. **`test_progress_store`** — verify `loadProgress()`/`saveProgress()` round-trip:
   - Write a `ReadingProgress` with known values to a temp file
   - Read it back
   - Verify byte-for-byte match
   - Use `FILE_O_WRITE` / `FILE_O_READ` on `SdFat`-compatible in-memory block or a temp file on the native build's filesystem

3. **`test_epub_parser_cache`** — (if miniz is testable in native):
   - Create a minimal test EPUB zip in memory
   - Call `EpubParser::open()` and `getChapter(0)`
   - Verify content is non-empty

Run with: `pio test -e native -f test_reader`

### Hardware Regression (MCP test harness)

After flashing `t5s3-epaper-v2-reader` to a device:

1. **Pre-condition check**: SD card with ≥1 `.epub` file in `/sd/`
2. **Test**: Open a book, navigate 3+ pages, note chapter+offset, power-cycle the device
3. **Assert**: On relaunch, reader opens at the same chapter and offset (within 1 page)
4. **Log check**: `ReaderFSM` logs should show `Loaded progress: chapter=N offset=M`

---

## 5. Rollback Plan

If Phase 2 destabilises the reader:

1. **Revert to prior build**: `pio run -e t5s3-epaper-v2-reader -t clean && pio run -e t5s3-epaper-v2-reader` (no code changes needed — the revert is just rebuilding the previous commit)
2. **SD card**: No persistent state is written to the firmware partition. All cache/state is on SD under `/.crosspoint/` — deleting that directory is the equivalent of a "reset reader state"
3. **Specific rollback**: If only `progress.bin` is causing issues, `ReaderFSM` can be edited to skip `loadProgress()` and `saveProgress()` by making them no-ops until fixed

---

## 6. Risks

| Risk                                                 | Probability | Impact                                                  | Mitigation                                                                                                    |
| ---------------------------------------------------- | ----------- | ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| SD card not initialised when reader opens            | Medium      | Reader hangs or shows "no books" even with card present | Add `SD.cardType()` check in `scanForBooks()` before `opendir`                                                |
| Cache write fails (SD write error)                   | Low         | Next page turn re-parses from ZIP; no data loss         | Catch file errors; log warning; fall back to ZIP extraction                                                   |
| Hash collision on cache key                          | Very Low    | Wrong book content served                               | `std::hash<std::string>` is 64-bit on ESP32; acceptable risk for Phase 1                                      |
| `progress.bin` corrupt read                          | Low         | Default to chapter 0, page 0                            | Validate struct bytes on load; if invalid, treat as no-saved-progress                                         |
| Large chapter causes OOM on ZIP extract              | Medium      | Device crashes                                          | Add `ESP.getFreeHeap()` check before `mz_zip_reader_extract_file_to_heap`; if < 64KB free, log error and skip |
| `saveProgress()` on every page turn floods SD writes | Low         | SD wear + lag on page turns                             | Debounce: only write if `PROGRESS_SAVE_DEBOUNCE_MS` (2000ms) has elapsed                                      |

---

_Plan created: May 2026_
_Awaiting approval before Phase 2 implementation_
