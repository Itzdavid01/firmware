#include "EInkParallelDisplay.h"

#ifdef USE_EINK_PARALLELDISPLAY

#include "Wire.h"
#include "variant.h"
#include <Arduino.h>
#include <atomic>
#include <stdlib.h>
#include <string.h>

#include "FastEPD.h"

// Thresholds for choosing partial vs full update
#ifndef EPD_PARTIAL_THRESHOLD_ROWS
#define EPD_PARTIAL_THRESHOLD_ROWS 128 // if changed region <= this many rows, prefer partial
#endif
#ifndef EPD_FULLSLOW_PERIOD
#define EPD_FULLSLOW_PERIOD 100 // every N full updates do a slow (CLEAR_SLOW) full refresh
#endif
#ifndef EPD_RESPONSIVE_MIN_MS
#define EPD_RESPONSIVE_MIN_MS 1000 // simple rate-limit (ms) for responsive updates
#endif

EInkParallelDisplay::EInkParallelDisplay(uint16_t width, uint16_t height, EpdRotation rot) : epaper(nullptr), rotation(rot)
{
    LOG_INFO("init EInkParallelDisplay");
    this->panelWidth = width;
    this->panelHeight = height;
    // Set dimensions in OLEDDisplay base class
    this->geometry = GEOMETRY_RAWMODE;

    if (rotation == EPD_ROT_PORTRAIT || rotation == EPD_ROT_INVERTED_PORTRAIT) {
        this->displayWidth = height;
        this->displayHeight = width;
    } else {
        this->displayWidth = width;
        this->displayHeight = height;
    }

#ifdef EPD_PADDING
    this->displayWidth -= 2 * EPD_PADDING;
    this->displayHeight -= 2 * EPD_PADDING;
#endif

    // Round shortest side up to nearest byte, to prevent truncation causing an undersized buffer
    uint16_t shortSide = min(this->displayWidth, this->displayHeight);
    uint16_t longSide = max(this->displayWidth, this->displayHeight);
    if (shortSide % 8 != 0)
        shortSide = (shortSide | 7) + 1;

    this->displayBufferSize = longSide * (shortSide / 8);

#ifdef EINK_LIMIT_GHOSTING_PX
    // allocate dirty pixel buffer same size as epaper buffers (panelRowBytes * panelHeight)
    size_t panelRowBytes = (this->panelWidth + 7) / 8;
    dirtyPixelsSize = panelRowBytes * this->panelHeight;
    dirtyPixels = (uint8_t *)calloc(dirtyPixelsSize, 1);
    ghostPixelCount = 0;
#endif
}

EInkParallelDisplay::~EInkParallelDisplay()
{
#ifdef EINK_LIMIT_GHOSTING_PX
    if (dirtyPixels) {
        free(dirtyPixels);
        dirtyPixels = nullptr;
    }
#endif
    // If an async full update is running, wait for it to finish
    if (asyncFullRunning.load()) {
        // wait a short while for task to finish
        for (int i = 0; i < 50 && asyncFullRunning.load(); ++i) {
            delay(50);
        }
        if (asyncTaskHandle) {
            // Let it finish or delete it
            vTaskDelete(asyncTaskHandle);
            asyncTaskHandle = nullptr;
        }
    }

    delete epaper;
}

/*
 * Called by the OLEDDisplay::init() path.
 */
bool EInkParallelDisplay::connect()
{
    LOG_INFO("Do EPD init");
    if (!epaper) {
        epaper = new FASTEPD;
#if defined(T5_S3_EPAPER_PRO_V1)
        epaper->initPanel(BB_PANEL_LILYGO_T5PRO, 28000000);
#elif defined(T5_S3_EPAPER_PRO_V2)
        epaper->initPanel(BB_PANEL_LILYGO_T5PRO_V2, 28000000);
        // initialize all port 0 pins (0-7) as outputs / HIGH
        for (int i = 0; i < 8; i++) {
            epaper->ioPinMode(i, OUTPUT);
            epaper->ioWrite(i, HIGH);
        }
#else
#error "unsupported EPD device!"
#endif
    }

    // epaper->setRotation(rotation); // does not work, messes up width/height
    epaper->setMode(BB_MODE_1BPP);
    epaper->clearWhite();
    epaper->fullUpdate(true);

#ifdef EINK_LIMIT_GHOSTING_PX
    // After a full/clear the dirty tracking should be reset
    resetGhostPixelTracking();
#endif

    return true;
}

/*
 * sendCommand - simple passthrough (not required for epd_driver-based path)
 */
void EInkParallelDisplay::sendCommand(uint8_t com)
{
    LOG_DEBUG("EInkParallelDisplay::sendCommand %d", (int)com);
}

/*
 * Start a background task that will perform a blocking fullUpdate(). This lets
 * display() return quickly while the heavy refresh runs in the background.
 */
void EInkParallelDisplay::startAsyncFullUpdate(int clearMode)
{
    if (asyncFullRunning.load())
        return; // already running

    asyncFullRunning.store(true);
    // pass 'this' as parameter
    BaseType_t rc = xTaskCreatePinnedToCore(EInkParallelDisplay::asyncFullUpdateTask, "epd_full", 4096 / sizeof(StackType_t),
                                            this, 2, &asyncTaskHandle,
#if CONFIG_FREERTOS_UNICORE
                                            0
#else
                                            1
#endif
    );
    if (rc != pdPASS) {
        LOG_WARN("Failed to create async full-update task, falling back to blocking update");
        epaper->fullUpdate(clearMode, false);
        epaper->backupPlane();
        asyncFullRunning.store(false);
        asyncTaskHandle = nullptr;
    }
}

/*
 * FreeRTOS task entry: runs the full update and then backs up plane.
 */
void EInkParallelDisplay::asyncFullUpdateTask(void *pvParameters)
{
    EInkParallelDisplay *self = static_cast<EInkParallelDisplay *>(pvParameters);
    if (!self) {
        vTaskDelete(nullptr);
        return;
    }

    // choose CLEAR_SLOW occasionally
    int clearMode = CLEAR_FAST;
    if (self->fastRefreshCount >= EPD_FULLSLOW_PERIOD) {
        clearMode = CLEAR_SLOW;
        self->fastRefreshCount = 0;
    } else {
        // when running async full, treat it as a full so reset fast count
        self->fastRefreshCount = 0;
    }

    self->epaper->fullUpdate(clearMode, false);
    self->epaper->backupPlane();

#ifdef EINK_LIMIT_GHOSTING_PX
    // A full refresh clears ghosting state
    self->resetGhostPixelTracking();
#endif

    self->asyncFullRunning.store(false);
    self->asyncTaskHandle = nullptr;

    // delete this task
    vTaskDelete(nullptr);
}

/*
 * Convert the OLEDDisplay buffer (vertical byte layout) into the 1bpp horizontal-bytes
 * buffer used by the FASTEPD library. For performance we write directly into FASTEPD's
 * currentBuffer() while comparing against previousBuffer() to detect changed rows.
 * After conversion we call FASTEPD::partialUpdate() or FASTEPD::fullUpdate() according
 * to a heuristic so only the minimal region is refreshed.
 */
void EInkParallelDisplay::display(void)
{
    const uint16_t w = this->displayWidth;
    const uint16_t h = this->displayHeight;

    // Simple rate limiting: avoid very-frequent responsive updates
    uint32_t nowMs = millis();
    if (lastUpdateMs != 0 && (nowMs - lastUpdateMs) < EPD_RESPONSIVE_MIN_MS) {
        LOG_DEBUG("rate-limited, skipping update");
        return;
    }

    // bytes per row in epd format (one byte = 8 horizontal pixels)
    const uint32_t panelRowBytes = (this->panelWidth + 7) / 8;

    // Get pointers to internal buffers
    uint8_t *cur = epaper->currentBuffer();
    const uint8_t *prev = epaper->previousBuffer(); // may be NULL on first init

    // Track changed physical row range while converting
    int min_ty = (int)panelHeight;
    int max_ty = -1;

#ifdef FAST_EPD_PARTIAL_UPDATE_BUG
    // Track changed physical byte column range
    int min_tx_byte = (int)panelRowBytes;
    int max_tx_byte = -1;
#endif

    // Compute a quick hash of the incoming OLED buffer (so we can skip identical frames)
    uint32_t imageHash = 0;
    uint32_t stride = (h + 7) / 8;
    for (uint32_t bi = 0; bi < w * stride; ++bi) {
        imageHash ^= ((uint32_t)buffer[bi]) << (bi & 31);
    }
    if (imageHash == previousImageHash) {
        // LOG_DEBUG("image identical to previous, skipping update");
        return;
    }

#ifdef EINK_LIMIT_GHOSTING_PX
    ghostPixelCount = 0;
#endif

    if (rotation == EPD_ROT_LANDSCAPE) {
        const uint32_t rowBytes = (w + 7) / 8;
        for (uint32_t y = 0; y < h; ++y) {
            const uint32_t base = (y >> 3) * w;               // (y/8) * width
            const uint8_t bitMask = (uint8_t)(1u << (y & 7)); // mask for this row in vertical-byte layout
            uint32_t ty = y;
#ifdef EPD_PADDING
            ty += EPD_PADDING;
#endif
            const uint32_t rowBase = ty * panelRowBytes;

            // process full 8-pixel bytes
            for (uint32_t xb = 0; xb < rowBytes; ++xb) {
                uint32_t x0 = xb * 8;
                // read up to 8 source bytes (vertical-byte per column)
                uint8_t b0 = (x0 + 0 < w) ? buffer[base + x0 + 0] : 0;
                uint8_t b1 = (x0 + 1 < w) ? buffer[base + x0 + 1] : 0;
                uint8_t b2 = (x0 + 2 < w) ? buffer[base + x0 + 2] : 0;
                uint8_t b3 = (x0 + 3 < w) ? buffer[base + x0 + 3] : 0;
                uint8_t b4 = (x0 + 4 < w) ? buffer[base + x0 + 4] : 0;
                uint8_t b5 = (x0 + 5 < w) ? buffer[base + x0 + 5] : 0;
                uint8_t b6 = (x0 + 6 < w) ? buffer[base + x0 + 6] : 0;
                uint8_t b7 = (x0 + 7 < w) ? buffer[base + x0 + 7] : 0;

                // build output byte: MSB = leftmost pixel
                uint8_t out = 0;
                if (b0 & bitMask)
                    out |= 0x80;
                if (b1 & bitMask)
                    out |= 0x40;
                if (b2 & bitMask)
                    out |= 0x20;
                if (b3 & bitMask)
                    out |= 0x10;
                if (b4 & bitMask)
                    out |= 0x08;
                if (b5 & bitMask)
                    out |= 0x04;
                if (b6 & bitMask)
                    out |= 0x02;
                if (b7 & bitMask)
                    out |= 0x01;

                // Bitwise inversion: ensure white background (0=white, 1=black)
                out = ~out;

                // handle partial byte at end of row by masking off invalid bits
                uint8_t mask = 0xFF;
                uint32_t bitsRemain = (w > x0) ? (w - x0) : 0;
                if (bitsRemain < 8) {
                    mask = (uint8_t)(0xFF << (8 - bitsRemain));
                    out &= mask;
                }

                uint32_t tx = x0;
#ifdef EPD_PADDING
                tx += EPD_PADDING;
#endif
                uint32_t pos = rowBase + (tx / 8);
                uint8_t prevVal = prev ? (prev[pos] & mask) : 0x00;
                bool changed = (prev == nullptr) || (prevVal != out);

                if (changed) {
                    if ((int)ty < min_ty)
                        min_ty = (int)ty;
                    if ((int)ty > max_ty)
                        max_ty = (int)ty;
#ifdef FAST_EPD_PARTIAL_UPDATE_BUG
                    int xb_p = (int)(tx / 8);
                    if (xb_p < min_tx_byte)
                        min_tx_byte = xb_p;
                    if (xb_p > max_tx_byte)
                        max_tx_byte = xb_p;
#endif
#ifdef EINK_LIMIT_GHOSTING_PX
                    if (prev)
                        markDirtyBits(prev, pos, mask, out);
#endif
                }
                // Always write the computed value into the current buffer
                cur[pos] = (cur[pos] & ~mask) | out;
            }
        }
    } else {
        // Generic / Rotation-aware pixel loop
        int rw = (rotation == EPD_ROT_PORTRAIT || rotation == EPD_ROT_INVERTED_PORTRAIT) ? h : w;
        int rh = (rotation == EPD_ROT_PORTRAIT || rotation == EPD_ROT_INVERTED_PORTRAIT) ? w : h;
        int offsetX = (panelWidth - rw) / 2;
        int offsetY = (panelHeight - rh) / 2;

        for (uint32_t vy = 0; vy < h; ++vy) {
            for (uint32_t vx = 0; vx < w; ++vx) {
                bool pixel = buffer[vx + (vy / 8) * w] & (1 << (vy % 8));
                bool val = !pixel; // Inversion: 0 is white

                int tx, ty;
                if (rotation == EPD_ROT_INVERTED_PORTRAIT) {
                    tx = vy;
                    ty = (w - 1) - vx;
                } else if (rotation == EPD_ROT_PORTRAIT) {
                    tx = (h - 1) - vy;
                    ty = vx;
                } else if (rotation == EPD_ROT_INVERTED_LANDSCAPE) {
                    tx = (w - 1) - vx;
                    ty = (h - 1) - vy;
                } else { // fallback
                    tx = vx;
                    ty = vy;
                }

                tx += offsetX;
                ty += offsetY;

                if (tx < 0 || tx >= (int)panelWidth || ty < 0 || ty >= (int)panelHeight)
                    continue;

                uint32_t pos = ty * panelRowBytes + (tx / 8);
                uint8_t bitMask = 0x80 >> (tx % 8);
                uint8_t oldVal = cur[pos];
                uint8_t newVal = val ? (oldVal | bitMask) : (oldVal & ~bitMask);

                if (newVal != oldVal) {
                    cur[pos] = newVal;
                    bool changed = (prev == nullptr) || ((newVal & bitMask) != (prev[pos] & bitMask));
                    if (changed) {
                        if ((int)ty < min_ty)
                            min_ty = (int)ty;
                        if ((int)ty > max_ty)
                            max_ty = (int)ty;
#ifdef FAST_EPD_PARTIAL_UPDATE_BUG
                        int xb_p = tx / 8;
                        if (xb_p < min_tx_byte)
                            min_tx_byte = xb_p;
                        if (xb_p > max_tx_byte)
                            max_tx_byte = xb_p;
#endif
#ifdef EINK_LIMIT_GHOSTING_PX
                        if (prev)
                            markDirtyBits(prev, pos, bitMask, newVal);
#endif
                    }
                }
            }
        }
    }

    // If nothing changed, avoid any panel update
    if (max_ty < 0) {
        // LOG_DEBUG("no pixel changes detected, skipping update (conv)");
        previousImageHash = imageHash;
        return;
    }

    // Choose partial vs full update using heuristic
    bool forceFull = (fastRefreshCount >= EPD_FULLSLOW_PERIOD);

#ifdef EINK_LIMIT_GHOSTING_PX
    if (ghostPixelCount > ghostPixelLimit) {
        LOG_WARN("ghost pixels %u > limit %u, forcing full refresh", ghostPixelCount, ghostPixelLimit);
        forceFull = true;
    }
#endif

    // Compute pixel bounds from min_ty/max_ty (already physical)
    int startRow = (min_ty / 8) * 8;
    int endRow = (max_ty / 8) * 8 + 7;

    LOG_DEBUG("EPD update rows=%d..%d alignedRows=%d..%d panelRowBytes=%u", min_ty, max_ty, startRow, endRow, panelRowBytes);

    if (epaper->getMode() == BB_MODE_1BPP && !forceFull && (max_ty - min_ty) <= EPD_PARTIAL_THRESHOLD_ROWS) {
#ifdef FAST_EPD_PARTIAL_UPDATE_BUG
        int startCol = (min_tx_byte <= max_tx_byte) ? (min_tx_byte * 8) : 0;
        int endCol = (min_tx_byte <= max_tx_byte) ? ((max_tx_byte + 1) * 8 - 1) : (panelWidth - 1);
        BB_RECT rect{startCol, startRow, endCol - startCol + 1, endRow - startRow + 1};
        epaper->fullUpdate(CLEAR_FAST, false, &rect);
#else
        epaper->partialUpdate(true, startRow, endRow);
#endif
        epaper->backupPlane();
        fastRefreshCount++;
    } else {
        startAsyncFullUpdate(forceFull ? CLEAR_SLOW : CLEAR_FAST);
    }

    lastUpdateMs = millis();
    previousImageHash = imageHash;
    lastDrawMsec = millis();
}

#ifdef EINK_LIMIT_GHOSTING_PX
// markDirtyBits: mark per-bit dirty flags and update ghostPixelCount
void EInkParallelDisplay::markDirtyBits(const uint8_t *prevBuf, uint32_t pos, uint8_t mask, uint8_t out)
{
    // defensive: need dirtyPixels allocated and prevBuf valid
    if (!dirtyPixels || !prevBuf)
        return;

    // 'out' is in FASTEPD polarity (1 = black, 0 = white)
    uint8_t newBlack = out & mask;    // bits that will be black now
    uint8_t newWhite = (~out) & mask; // bits that will be white now

    // previously recorded dirty bits for this byte
    uint8_t before = dirtyPixels[pos];

    // Ghost bits: bits that were previously marked dirty and are now being driven white
    uint8_t ghostBits = before & newWhite;
    if (ghostBits) {
        ghostPixelCount += __builtin_popcount((unsigned)ghostBits);
    }

    // Only mark bits dirty when they turn black now (accumulate until a full refresh)
    uint8_t newlyDirty = newBlack & (~before);
    if (newlyDirty) {
        dirtyPixels[pos] |= newlyDirty;
    }
}

// reset ghost tracking (call after a full refresh)
void EInkParallelDisplay::resetGhostPixelTracking()
{
    if (!dirtyPixels)
        return;
    memset(dirtyPixels, 0, dirtyPixelsSize);
    ghostPixelCount = 0;
}
#endif

/*
 * forceDisplay: use lastDrawMsec
 */
bool EInkParallelDisplay::forceDisplay(uint32_t msecLimit)
{
    uint32_t now = millis();
    if (lastDrawMsec == 0 || (now - lastDrawMsec) > msecLimit) {
        display();
        return true;
    }
    return false;
}

void EInkParallelDisplay::endUpdate()
{
    {
        // ensure any async full update is started/completed
        if (asyncFullRunning.load()) {
            // nothing to do; background task will run and call backupPlane when done
        } else {
            epaper->fullUpdate(CLEAR_FAST, false);
            epaper->backupPlane();
#ifdef EINK_LIMIT_GHOSTING_PX
            resetGhostPixelTracking();
#endif
        }
    }
}

#endif