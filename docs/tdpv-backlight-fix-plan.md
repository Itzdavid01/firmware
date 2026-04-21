# Fix: T-Deck Pro Voice — Backlight Always-On + Alt+B Toggle

## Status: FIXED (2026-04-21)

Both bugs shared the same root cause and were resolved in a single commit.

---

## Root Cause

`TCA8418KeyboardBase` declares member functions:

```cpp
bool digitalWrite(uint8_t pinnum, uint8_t level);
bool pinMode(uint8_t pinnum, uint8_t mode);
```

These operate on the TCA8418 I/O expander chip, not ESP32 GPIOs. They
**shadow Arduino's global `digitalWrite`/`pinMode`** inside any subclass.

Both methods early-return `false` when `pinnum > TCA8418_COL9` (17).

`KB_BL_PIN = 42` → every call to `digitalWrite(KB_BL_PIN, ...)` or
`pinMode(KB_BL_PIN, ...)` inside `TDeckProKeyboard` was a **silent no-op**.

---

## Bug 1: Backlight Always On

`earlyInitVariant()` in `variants/esp32s3/t-deck-pro-voice/variant.cpp`
set GPIO 42 HIGH as part of an exploratory "try all likely power pins" block.

`TDeckProKeyboard::reset()` called `setBacklight(false)` to turn it off —
but `setBacklight()` called `TCA8418KeyboardBase::digitalWrite(42, LOW)`,
which silently did nothing. GPIO 42 stayed HIGH. Backlight stayed on.

## Bug 2: Alt+B Does Nothing

`toggleBacklight()` → `setBacklight(!_bl_on)` → `TCA8418KeyboardBase::digitalWrite(42, ...)` → no-op.

The tap map, modifier logic, and `released()` dispatch were all correct.
The action reached `toggleBacklight()` but the GPIO never changed.

---

## Fix

**`src/input/TDeckProKeyboard.cpp`** — use `::` to call Arduino's globals:

```cpp
void TDeckProKeyboard::reset()
{
    TCA8418KeyboardBase::reset();
    ::pinMode(KB_BL_PIN, OUTPUT);   // was: pinMode(...)  → TCA8418 shadow, no-op
    setBacklight(false);
}

void TDeckProKeyboard::setBacklight(bool on)
{
    _bl_on = on;
    ::digitalWrite(KB_BL_PIN, on ? HIGH : LOW);  // was: digitalWrite(...) → TCA8418 shadow, no-op
}
```

**`variants/esp32s3/t-deck-pro-voice/variant.cpp`** — remove GPIO 42 from
`earlyInitVariant()`. Kept only audio-critical pins:

| Pin | Purpose | Keep? |
|-----|---------|-------|
| 10  | Unknown (experimental) | Removed |
| 41  | Audio power enable (`BOARD_6609_EN`) | **Kept** |
| 42  | `KB_BL_PIN` — backlight, set by keyboard driver | Removed |
| 39  | Not needed (rejected by I2S driver for MCLK) | Removed |
| 21  | `DAC_I2S_MCLK` — I2S driver requires valid GPIO | **Kept** |

---

## Audio Safety

Changes touch only GPIO 42. Audio uses GPIOs 7 (BCK), 8 (DOUT), 9 (WS),
21 (MCLK). No overlap. No audio risk.

---

## Verification

- Boot: backlight off
- Alt+B: toggles backlight on/off
- Boot melody plays (audio unaffected)
