# T5S3 InkHUD ReaderApplet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the standalone `ReaderFSM` with an InkHUD `Applet`-based `ReaderApplet` so the e-reader runs as a tile within the InkHUD UI framework on the T5S3 e-ink display.

**Architecture:** The existing `ReaderFSM` state machine (BOOK_LIST, READING, SUSPENDED) is preserved but refactored onto the `Applet` lifecycle (activate/deactivate, foreground/background, onRender). The `EpubParser`/`ChapterCache`/`ProgressStore` layer is unchanged - only the display/input scaffolding moves from raw `screen->getDisplayDevice()` calls to InkHUD's `Applet::drawPixel()` / `X()` / `Y()` coordinate system. Display updates are driven by `requestUpdate(EInk::UpdateTypes)`.

**Tech Stack:** InkHUD `Applet` base class, `GFX` (AdafruitGFX-compatible), `Tile` → `Renderer` → `ED047TC1Parallel` E-Ink driver, `Inputs::TwoButton` for input, `gps/RTC.h` for timestamps.

---

## Root Cause Summary (from boot loop diagnosis)

1. **Wrong env flashed:** `t5s3-epaper-v2-reader` (standalone reader) was flashed, which sets `HAS_SCREEN=1` and creates `graphics::Screen` - but without InkHUD initialization → boot loop.
2. **`t5s3_epaper_inkhud` excluded reader:** `build_src_filter` has `-<reader/>`, so the reader was never compiled in that env.
3. **No single env combines InkHUD + reader.** The fix requires a merged env + ReaderFSM → ReaderApplet refactor.

---

## File Map

| File                                           | Action        | Responsibility                                                    |
| ---------------------------------------------- | ------------- | ----------------------------------------------------------------- |
| `src/reader/ReaderApplet.h`                    | **CREATE**    | ReaderApplet class (inherits `Applet`)                            |
| `src/reader/ReaderApplet.cpp`                  | **CREATE**    | Lifecycle hooks, input handlers, render pipeline                  |
| `src/reader/ReaderFSM.h`                       | **MODIFY**    | Rename `ReaderFSM` → `ReaderController` (extracted state machine) |
| `src/reader/ReaderFSM.cpp`                     | **MODIFY**    | Rename, strip display/input scaffolding (now in ReaderApplet)     |
| `src/reader/EpubParser.h/.cpp`                 | **UNCHANGED** | Cache-first EPUB parser                                           |
| `src/reader/ChapterCache.h/.cpp`               | **UNCHANGED** | FNV-1a cache key + paths                                          |
| `src/reader/ProgressStore.h/.cpp`              | **UNCHANGED** | 12-byte ReadingProgress persistence                               |
| `src/reader/DocumentRenderer.h/.cpp`           | **MODIFY**    | Replace `OLEDDisplay *` with `Applet *` drawing API               |
| `variants/esp32s3/t5s3_epaper/platformio.ini`  | **MODIFY**    | Add `t5s3_epaper_inkhud_reader` env (merged InkHUD + reader)      |
| `variants/esp32s3/t5s3_epaper/nicheGraphics.h` | **MODIFY**    | Replace standalone ReaderFSM init with ReaderApplet registration  |
| `src/graphics/niche/InkHUD/Applet.h`           | **READ-ONLY** | Reference for lifecycle, input mask, drawing API                  |
| `src/graphics/niche/InkHUD/Renderer.h/.cpp`    | **READ-ONLY** | Reference for `requestUpdate` / update types                      |

---

## Task Decomposition

### Task 1: Rename ReaderFSM → ReaderController

Extract the pure state machine from `ReaderFSM` into a new class `ReaderController` (no display, no input, no Applet dependency). This becomes the brain that `ReaderApplet` delegates to.

**Files:**

- Modify: `src/reader/ReaderFSM.h:1-45`
- Modify: `src/reader/ReaderFSM.cpp:1-200`

**Steps:**

- [ ] **Step 1: Add `ReaderController` class to `ReaderFSM.h`**

Add after the existing includes (before the `#endif` guard closing):

```cpp
// Forward-declare Applet to avoid circular dep
class ReaderApplet;

// Pure state-machine extracted from ReaderFSM - no display, no input, no Applet dependency.
class ReaderController
{
  public:
    static ReaderController &instance();

    // Book list
    void scanForBooks();
    const std::vector<std::string> &getBookFiles() const;
    int getSelectedBookIndex() const;
    void selectNextBook();
    void selectPrevBook();

    // Reading
    bool openBook(size_t index);           // returns false on error
    void closeBook();
    bool isBookOpen() const;
    const std::string &getOpenBookPath() const;
    int getCurrentChapter() const;
    int getCurrentPageOffset() const;
    const std::string &getCurrentChapterText() const;
    int getChapterCount() const;

    // Navigation (call on input events from ReaderApplet)
    void nextPage();     // advance ~500 chars or next chapter
    void prevPage();     // retreat ~500 chars or prev chapter

    // Progress persistence (called by ReaderApplet on debounce/close)
    struct Progress { int chapterIndex; int pageOffset; uint32_t timestamp; };
    Progress getProgress() const;
    void setProgress(const Progress &p);

  private:
    friend class ReaderApplet;
    ReaderController();
    ReaderController(const ReaderController &) = delete;
    ReaderController &operator=(const ReaderController &) = delete;

    EpubParser parser;
    int currentChapter = 0;
    int currentPageOffset = 0;
    std::string currentChapterText;
    std::string openBookPath;
    std::vector<std::string> bookFiles;
    int selectedBookIndex = 0;
    uint32_t lastProgressSaveMs = 0;
    static constexpr uint32_t PROGRESS_SAVE_DEBOUNCE_MS = 2000;
};
```

- [ ] **Step 2: Move FSM methods from ReaderFSM.cpp to ReaderController**

Cut ALL private implementation from `ReaderFSM.cpp` - `scanForBooks()`, `handleInputEvent()`, `refreshDisplay()`, `saveProgress()`, `loadProgress()` - into a new `ReaderController` implementation block. `ReaderFSM.cpp` becomes a thin shim that delegates to `ReaderController::instance()`.

- [ ] **Step 3: Replace ReaderFSM class body in ReaderFSM.h**

Replace the old `ReaderFSM` class with a stub that delegates to `ReaderController`:

```cpp
// Thin wrapper - ReaderFSM is now a deprecated alias for ReaderController.
// All real logic lives in ReaderController.
class ReaderFSM
{
  public:
    static ReaderFSM *getInstance() { return &instance(); }
    void init() { instance().scanForBooks(); }  // was scanForBooks in init()
    void launch() { /* no-op: Applet lifecycle handles this */ }
    void update() {}
    void nextTile() {}
    void shortpress() {}
    void longpress() {}

  private:
    // Keep old type name alive for any stale callers - delegate to singleton
    static ReaderController &instance();
};
```

- [ ] **Step 4: Build to verify rename compiles**

Run: `pio run -e t5s3-epaper-v2-reader 2>&1 | grep -E "error|warning: 'ReaderFSM'" | head -20`
Expected: Clean build (warnings about `ReaderFSM` → `ReaderController` are OK, they are the migration).

---

### Task 2: Create ReaderApplet (the InkHUD Applet shell)

**Files:**

- Create: `src/reader/ReaderApplet.h`
- Create: `src/reader/ReaderApplet.cpp`

**Steps:**

- [ ] **Step 1: Write ReaderApplet.h**

```cpp
#pragma once
#include "graphics/niche/InkHUD/Applet.h"
#include "ReaderFSM.h"  // ReaderController lives here now

namespace reader
{

// InkHUD Applet wrapping the ReaderController.
// Runs as a tile within the InkHUD UI - no direct screenHW access.
// All drawing goes through Applet::drawPixel() → Tile → Renderer.
class ReaderApplet : public NicheGraphics::InkHUD::Applet
{
  public:
    ReaderApplet();

    // ── Applet lifecycle ─────────────────────────────────
    void onActivate() override;
    void onDeactivate() override;
    void onForeground() override;
    void onBackground() override;
    void onRender(bool full) override;

    // ── Input handlers ──────────────────────────────────
    void onButtonShortPress() override;   // SELECT → open book / advance page
    void onButtonLongPress() override;     // BACK → return to book list
    void onNavDown() override;             // scroll book list down / next page
    void onNavUp() override;               // scroll book list up / prev page

    const char *getName() const override { return "Reader"; }

  private:
    enum class View { BOOK_LIST, READING };
    View currentView = View::BOOK_LIST;

    // Debounce progress saves (2s)
    uint32_t lastSaveMs = 0;
    static constexpr uint32_t PROGRESS_SAVE_DEBOUNCE_MS = 2000;

    void refreshBookList();
    void refreshReading();
    void saveProgressDebounced();

    // Input subscription bitmask
    static constexpr uint8_t INPUTS = BUTTON_SHORT | BUTTON_LONG | NAV_UP | NAV_DOWN;
};

} // namespace reader
```

- [ ] **Step 2: Write ReaderApplet.cpp**

```cpp
#include "ReaderApplet.h"
#include "gps/RTC.h"
#include <algorithm>

namespace reader
{

ReaderApplet::ReaderApplet()
{
    name = "Reader";
    setInputsSubscribed(INPUTS, true);
    // Font is set globally via Applet::fontLarge/fontMedium in nicheGraphics.h
}

void ReaderApplet::onActivate()
{
    // Controller is a singleton - ensure it's scanned
    ReaderController::instance().scanForBooks();
}

void ReaderApplet::onDeactivate()
{
    // Save progress before deactivating
    auto p = ReaderController::instance().getProgress();
    ReaderController::instance().setProgress(p);  // triggers write
}

void ReaderApplet::onForeground()
{
    // Refresh the view immediately
    requestUpdate(Drivers::EInk::UpdateTypes::FAST, true);
}

void ReaderApplet::onBackground()
{
    // Save on exit
    auto p = ReaderController::instance().getProgress();
    ReaderController::instance().setProgress(p);
}

void ReaderApplet::onRender(bool /*full*/)
{
    resetDrawingSpace();
    switch (currentView) {
        case View::BOOK_LIST: refreshBookList(); break;
        case View::READING:   refreshReading();   break;
    }
}

void ReaderApplet::onButtonShortPress()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::BOOK_LIST) {
        if (!ctrl.getBookFiles().empty()) {
            if (ctrl.openBook(ctrl.getSelectedBookIndex())) {
                currentView = View::READING;
                requestUpdate(Drivers::EInk::UpdateTypes::FAST, true);
            }
        }
    } else {
        // READING: short press = next page
        ctrl.nextPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::onButtonLongPress()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::READING) {
        ctrl.closeBook();
        currentView = View::BOOK_LIST;
    } else {
        // BOOK_LIST: long press = exit reader applet (→ return to InkHUD tile)
        ctrl.closeBook();
        sendToBackground();  // Applet base class method
        inkhud->prevApplet();
        return;
    }
    requestUpdate(Drivers::EInk::UpdateTypes::FAST, true);
}

void ReaderApplet::onNavDown()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::BOOK_LIST) {
        ctrl.selectNextBook();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
    } else {
        // READING: down = next page
        ctrl.nextPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::onNavUp()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::BOOK_LIST) {
        ctrl.selectPrevBook();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
    } else {
        // READING: up = prev page
        ctrl.prevPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::refreshBookList()
{
    // Fill background white
    fillRect(0, 0, width(), height(), WHITE);

    // Title
    drawHeader("E-Reader");

    // Book list (max 8 entries visible)
    auto &books = ReaderController::instance().getBookFiles();
    int selected = ReaderController::instance().getSelectedBookIndex();
    int startY = getHeaderHeight() + 8;

    if (books.empty()) {
        printAt(X(0.05f), Y(0.3f), "No books found.");
        printAt(X(0.05f), Y(0.4f), "Copy .epub to /sd/");
        return;
    }

    for (int i = 0; i < (int)books.size() && i < 8; i++) {
        bool sel = (i == selected);
        int16_t y = startY + i * (fontMedium.height + 4);
        if (sel) {
            // Highlight selected row
            fillRect(0, y, width(), fontMedium.height + 4, BLACK);
            setTextColor(WHITE);
        } else {
            setTextColor(BLACK);
        }
        printAt(4, y, books[i].substr(books[i].find_last_of('/') + 1));
    }
    setTextColor(BLACK);
}

void ReaderApplet::refreshReading()
{
    auto &ctrl = ReaderController::instance();

    fillRect(0, 0, width(), height(), WHITE);

    // Header: book name + chapter indicator
    std::string bookName = ctrl.getOpenBookPath();
    size_t slash = bookName.find_last_of('/');
    if (slash != std::string::npos) bookName = bookName.substr(slash + 1);
    char header[64];
    snprintf(header, sizeof(header), "%s %d/%d", bookName.c_str(),
             ctrl.getCurrentChapter() + 1, ctrl.getChapterCount());
    drawHeader(header);

    // Chapter text - wrapped rendering
    const std::string &text = ctrl.getCurrentChapterText();
    int offset = ctrl.getCurrentPageOffset();

    // Skip to offset
    if (offset > 0 && offset < (int)text.size()) {
        int lineStart = offset;
        // Align to start of word
        while (lineStart > 0 && text[lineStart - 1] != ' ' && text[lineStart - 1] != '\n')
            lineStart--;
        std::string displayText = text.substr(lineStart);

        int16_t textTop = getHeaderHeight() + 6;
        uint16_t maxW = width() - 8;
        uint32_t h = getWrappedTextHeight(4, textTop, maxW, displayText);
        printWrapped(4, textTop, maxW, displayText);
    } else {
        printAt(X(0.05f), Y(0.3f), "[empty]");
    }
}

void ReaderApplet::saveProgressDebounced()
{
    uint32_t now = millis();
    if (now - lastSaveMs >= PROGRESS_SAVE_DEBOUNCE_MS) {
        lastSaveMs = now;
        auto p = ReaderController::instance().getProgress();
        ReaderController::instance().setProgress(p);
    }
}

} // namespace reader
```

- [ ] **Step 3: Build to verify ReaderApplet compiles**

Run: `pio run -e t5s3-epaper-v2-reader 2>&1 | grep -E "error:" | head -20`
Expected: Errors only if ReaderController refactor has issues (fix inline).

---

### Task 3: Update DocumentRenderer to use Applet drawing API

**Files:**

- Modify: `src/reader/DocumentRenderer.h`
- Modify: `src/reader/DocumentRenderer.cpp`

**Steps:**

- [ ] **Step 1: Replace OLEDDisplay dependency with Applet pointer**

`DocumentRenderer` currently takes `OLEDDisplay *` and calls `display->drawStringMaxWidth()`. Replace with an `Applet *` and use `Applet::printWrapped()` which already handles word-wrap. This eliminates the OLEDDisplay.h dependency and makes it InkHUD-compatible.

Update `DocumentRenderer.h`:

```cpp
// Before:
void renderPage(OLEDDisplay *display, const std::string &text, int pageOffset);
int calculatePages(OLEDDisplay *display, const std::string &text);

// After:
void renderPage(NicheGraphics::InkHUD::Applet *applet, const std::string &text, int pageOffset);
int calculatePages(NicheGraphics::InkHUD::Applet *applet, const std::string &text);
```

- [ ] **Step 2: Rewrite renderPage to use Applet API**

```cpp
// Before (OLEDDisplay-based):
void DocumentRenderer::renderPage(OLEDDisplay *display, const std::string &text, int pageOffset)
{
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setColor(BLACK);
    display->clear();
    // ... drawStringMaxWidth with pointer arithmetic ...
    display->display();
}

// After (Applet-based):
void DocumentRenderer::renderPage(NicheGraphics::InkHUD::Applet *applet, const std::string &text, int pageOffset)
{
    applet->resetDrawingSpace();
    applet->setTextColor(BLACK);

    // Strip HTML in-place
    std::string clean;
    clean.reserve(text.size());
    bool inTag = false;
    for (char c : text) {
        if (c == '<') { inTag = true; continue; }
        if (c == '>') { inTag = false; continue; }
        if (!inTag) clean.push_back(c);
    }

    if (pageOffset > 0 && pageOffset < (int)clean.size()) {
        // Align to word start
        int lineStart = pageOffset;
        while (lineStart > 0 && clean[lineStart - 1] != ' ' && clean[lineStart - 1] != '\n')
            lineStart--;
        clean = clean.substr(lineStart);
    }

    int16_t top = applet->getHeaderHeight() + 6;
    uint16_t w = applet->X(1.0f) - 8;
    applet->printWrapped(4, top, w, clean);
}
```

- [ ] **Step 3: Build to verify DocumentRenderer compiles**

Run: `pio run -e t5s3-epaper-v2-reader 2>&1 | grep -E "error:" | head -20`
Expected: Clean or fixable errors.

---

### Task 4: Fix platformio.ini - add merged `t5s3_epaper_inkhud_reader` env

**Files:**

- Modify: `variants/esp32s3/t5s3_epaper/platformio.ini`

**Steps:**

- [ ] **Step 1: Add the merged env after `t5s3_epaper_inkhud`**

```ini
[env:t5s3_epaper_inkhud_reader]
extends = t5s3_epaper_base, inkhud
board_level = extra
board_check = false
build_flags =
  ${t5s3_epaper_base.build_flags}
  ${inkhud.build_flags}
  -D SDCARD_USE_SPI1
  -D T5_S3_EPAPER_PRO_V2
  -D HAS_READER
  -I src/reader
build_src_filter =
  ${t5s3_epaper_base.build_src_filter}
  ${inkhud.build_src_filter}
  +<reader/>
lib_deps =
  ${inkhud.lib_deps}
  ${t5s3_epaper_base.lib_deps}
  https://github.com/richgel999/miniz/archive/refs/tags/2.1.0.zip
```

Key differences from `t5s3_epaper_inkhud`:

- Remove `-<reader/>` from build_src_filter
- Add `+<reader/>` to include reader source
- Add `-D HAS_READER -I src/reader` build flags
- Add miniz zip dependency
- Remove board_level override (keep extra)

- [ ] **Step 2: Verify the env is recognized**

Run: `pio run -e t5s3_epaper_inkhud_reader --target envinfo 2>&1 | head -10`
Expected: Shows `t5s3_epaper_inkhud_reader` env (not error)

---

### Task 5: Update nicheGraphics.h - replace ReaderFSM with ReaderApplet

**Files:**

- Modify: `variants/esp32s3/t5s3_epaper/nicheGraphics.h`

**Steps:**

- [ ] **Step 1: Replace `#include "src/reader/ReaderFSM.h"` with `#include "src/reader/ReaderApplet.h"`**

In the include section (before `setupNicheGraphics()`):

```cpp
// Before:
// #include "src/reader/ReaderFSM.h"

// After:
#include "src/reader/ReaderApplet.h"
```

- [ ] **Step 2: Replace `inkhud->addApplet(&ReaderFSM)` call with `ReaderApplet`**

Find the existing `inkhud->addApplet` block and add the Reader applet. Add before `inkhud->begin()`:

```cpp
// Add before inkhud->begin()
// Reader applet - auto-shown so user can find it on the tile list
static reader::ReaderApplet *g_readerApplet = new reader::ReaderApplet();
inkhud->addApplet("Reader", g_readerApplet, true, false, 0);
```

Remove or comment out the old standalone reader initialization (if any `ReaderFSM::getInstance()` call exists).

- [ ] **Step 3: Remove old ReaderFSM init from main.cpp (if present)**

Check `src/main.cpp` for any `ReaderFSM::init()` or `ReaderFSM::launch()` call - these should be removed since the Applet lifecycle now handles initialization via `onActivate()`.

Run: `grep -n "ReaderFSM" src/main.cpp`
Expected: No results

- [ ] **Step 4: Verify InkHUD env still builds clean**

Run: `pio run -e t5s3_epaper_inkhud_reader 2>&1 | grep -E "^Error|error:|warning: unused" | head -20`
Expected: Build succeeds with no errors.

---

### Task 6: Flash and verify on T5S3 hardware

**Files:** (none - hardware test)

**Steps:**

- [ ] **Step 1: List connected devices**

Run: `pio device list`
Expected: Port for T5S3 (CP2102 or similar)

- [ ] **Step 2: Flash the merged env**

Run: `pio run -e t5s3_epaper_inkhud_reader -t upload --upload-port /dev/ttyUSB0`
(Use actual port from step above, confirm with operator first)

- [ ] **Step 3: Monitor serial output for boot loop**

Run: `pio device monitor --port /dev/ttyUSB0 --baud 115200 2>&1 | head -50`
Expected: No crash loop, InkHUD logo screen, "Reader" tile visible, no `PANIC` or ` Guru Meditation`

- [ ] **Step 4: Verify no boot loop (confirm with operator)**

Ask operator: Is the device stable on the InkHUD home screen?

---

## Self-Review Checklist

- [ ] **Spec coverage**: Every requirement from the boot loop diagnosis is addressed (wrong env, reader exclusion, single merged env, Applet refactor).
- [ ] **Placeholder scan**: No "TBD", "TODO", "implement later", or vague steps. All steps have concrete code or commands.
- [ ] **Type consistency**: `ReaderController` class name consistent across Tasks 1-3. `ReaderApplet::onRender`, `onButtonShortPress`, etc. all match the `Applet` virtual signatures. `InputMask` bits match actual input handlers implemented.
- [ ] **No duplicate work**: `EpubParser`, `ChapterCache`, `ProgressStore` left untouched in all tasks.
- [ ] **Build verification steps**: Each task includes a `pio run` command to verify compilation before moving to the next task.

---

## Execution Options

**Plan saved to:** `docs/superpowers/plans/2026-05-11-t5s3-inkhud-reader-applet.md`

**Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** - Execute tasks in this session using `superpowers:executing-plans`, batch execution with checkpoints.

Which approach?
