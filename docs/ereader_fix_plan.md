# T5-S3 E-Reader (InkHUD `Reader` applet) — Fix & Test Plan

Branch: `t5s3-epaper-ereader-poc` · Env: `t5s3_epaper_inkhud_reader` · Board: T5-S3 e-paper (ESP32-S3, `T5_S3_EPAPER_PRO_V2`).

This documents the bugs found while bringing up the e-reader, the fixes applied,
what is still open, and a concrete test matrix (reader features **and** switching
between Reader and other InkHUD applets).

## Build / flash / observe (how to work this device)

- Build: `mcp__meshtastic__build env=t5s3_epaper_inkhud_reader` (or `pio run -e t5s3_epaper_inkhud_reader`).
- Flash needs the serial port **exclusively**. A podman container `meshtastic-serial-bridge`
  (systemd user unit `serial-bridge.service`) runs `socat TCP-LISTEN:4403 ↔ /dev/ttyACM0`
  and holds the port. Stop it first:
  `systemctl --user stop serial-bridge.service` → `pio_flash` → (optionally) start it again.
- No clean panic backtrace over USB-CDC normally. To capture crashes, stop the bridge and read
  `/dev/ttyACM0` directly (pyserial, 115200; toggle RTS to reset). Decode with
  `xtensa-esp32s3-elf-addr2line -pfiaC -e <elf> <pc...>`.
- Screen is only visible via the laptop webcam (`/dev/video1`) — capture as root with cv2
  (user must be in `video` group). There is no on-device screen scrape.

## Root causes found this session (all fixed)

1. **BOOT button never wired (couldn't get past the tip / no input).**
   `variants/esp32s3/t5s3_epaper/nicheGraphics.h` only called `buttons->setWiring(0, …)`
   under `#if T5_S3_EPAPER_PRO_V1`; the V2 `#else` was empty, so the BOOT button (GPIO0)
   was never bound — `start()` attached an ISR to an unconfigured pin (`gpio_isr` error +
   `IO 0 is not set as GPIO` flood). **Fix:** wire button 0 to `BUTTON_PIN` with pull-up in
   the V2 branch.

2. **Crash (reset) on opening a book.**
   EPUB open runs synchronously on the Arduino `loopTask`; miniz
   `mz_zip_reader_extract_to_mem_no_alloc` puts a ~11 KB `tinfl_decompressor` on the stack,
   overflowing the default 8 KB loop stack → stack-canary panic. **Fix:**
   `-D ARDUINO_LOOP_STACK_SIZE=24576` in the reader env (`variants/esp32s3/t5s3_epaper/platformio.ini`).

3. **Raw XHTML rendered instead of prose.**
   `refreshReading()` drew the raw chapter; `DocumentRenderer::prepareChapterText` was unused.
   **Fix:** `htmlToText()` in `ReaderFSM.cpp` strips `<head>/<style>/<script>`, turns block
   elements into newlines, decodes common entities, normalizes smart-quote/dash/ellipsis
   UTF-8 to ASCII; called in `refreshChapterText()`.

4. **Font too small.** **Fix:** `setFont(fontMedium)` for body text in `refreshReading()`.

5. **"Remember last page" never worked (always page 1).**
   Arduino `SD` is mounted at `/sd` and `vfs_api.cpp` prepends the mountpoint, so the reader's
   `SD.open("/sd/.crosspoint/…")` resolved to **`/sd/sd/.crosspoint/…`** and every cache/progress
   write failed silently. (The EPUB still read because miniz uses POSIX `fopen("/sd/book.epub")`.)
   **Fix:** drop the `/sd` prefix from Arduino-SD paths in `ChapterCache.cpp` and
   `EpubParser::isSdCardPresent()`. Also clamp the restored offset to the (stripped) chapter
   length in `ReaderController::loadProgress()`.

   Files: `src/reader/ChapterCache.cpp`, `src/reader/EpubParser.cpp`, `src/reader/ReaderFSM.cpp`.

Also fixed earlier: InkHUD had no `notifyLightSleepEnd` observer, so the panel never repainted
on wake (`Events.{h,cpp}` — `afterLightSleep` forces a repaint on GPIO wake).

## Open / still-to-verify

- **Progress persistence** (fix #5) — verify on device (see tests T5–T7). If still failing,
  instrument `saveReadingProgress`/`loadReadingProgress`/`ensureCacheRootForEpub` with
  `LOG_INFO` of the resolved path + `SD.open`/`mkdir` results, capture serial, confirm the
  written path is `/sd/.crosspoint/…` (not `/sd/sd/…`).
- **Pagination quality** — `nextPage`/`prevPage` step a fixed 500 chars regardless of font;
  with `fontMedium` a "page" may overflow/clip at the bottom and skip text. Consider computing
  page size from rendered line metrics, or reducing the step.
- **Residual glyphs** — non-ASCII beyond the mapped punctuation still passes through to a font
  that may lack glyphs. Decide: extend the map, or hard-strip bytes ≥ 0x80.
- **Chapter cache** — same `/sd` fix should make `sections/<n>.bin` caching work; verify a cache
  hit on second open of a chapter (`EpubParser: cache hit chapter N`).
- **Deep-sleep side-key wake gap** (`main-esp32.cpp:245`, GPIO38 missing from ext1) — latent
  (sds disabled); fix if deep sleep is ever enabled.

## Test matrix

### Reader functionality

- **T1 Boot → tip dismiss:** BOOT short-press advances/closes the tip to the book list.
- **T2 Book list:** `.epub` files under `/sd` are listed; NAV up/down moves selection.
- **T3 Open book:** BOOT opens selected book → READING view, **no reset** (regression guard for #2).
- **T4 Render:** body is readable prose (no `<tags>`, no `<?xml`/CSS), `fontMedium`, smart
  quotes/dashes rendered as ASCII.
- **T5 Paginate:** BOOT / NAV-down advances a page, NAV-up goes back; chapter rolls over at ends
  (`getCurrentChapter()` increments; header shows `name C/N`).
- **T6 Resume after close:** read to page X → long-press BOOT (back to list) → reopen → resumes at
  page X. (Confirms `progress.bin` write+read.)
- **T7 Resume after reset:** read to page X, wait >2 s (debounced save), hard-reset device,
  reopen book → resumes near page X.
- **T8 Chapter cache:** open a chapter twice → second load logs `cache hit` (serial), faster.
- **T9 No SD / no books:** with no `.epub`, list shows "No books found / Copy .epub to /sd/".

### Applet switching (Reader ↔ others)

- **A1 Leave Reader:** from READING, long-press BOOT path → `closeBook()` (saves progress) →
  `sendToBackground()` → `prevApplet()`; confirm a _different_ applet (e.g. All Messages /
  Positions / Heard) renders and is interactive. No crash, no stale Reader frame.
- **A2 Return to Reader:** cycle applets back to Reader → `onActivate`/`onForeground` → resumes the
  open book at saved page (or book list if closed). No reset.
- **A3 Background save:** open book, switch away mid-chapter, switch back → page preserved
  (`onBackground` → `setProgress`).
- **A4 Sleep/wake during Reader:** let screen blank + light-sleep while a book is open; press
  BOOT → device wakes and **repaints** the reader page (regression guard for the InkHUD wake fix).
- **A5 Notification overlay:** receive a text message while reading → notification shows, then
  reader view restored on dismiss; no corruption.
- **A6 Menu:** long-press opens the InkHUD menu over/around the reader and closes cleanly back to
  the reader.

### Pass criteria

All of T1–T9 and A1–A6 with: no resets (capture serial for `Guru`/`Backtrace`), no `IO 0 is not
set as GPIO` flood, progress survives both close (T6) and reset (T7), and switching never leaves a
stale/blank frame.

## Verification procedure (per change)

1. `systemctl --user stop serial-bridge.service`
2. build + `pio_flash` (port `/dev/ttyACM0`)
3. Drive via physical buttons; capture screen via webcam (root + cv2) to confirm render.
4. For crash/persistence questions, read `/dev/ttyACM0` directly and grep for `Guru|Backtrace|
cache hit|cache miss|EpubParser`.
