# T-Deck Pro Voice — Haptic Feedback

## Status: keypress haptic REMOVED (2026-04-21)

---

## Hardware

DRV2605 haptic driver at I2C 0x5A. Enabled via:

```cpp
// variants/esp32s3/t-deck-pro-voice/variant.h
#define HAS_DRV2605
#define PIN_DRV_EN 2
```

`PIN_DRV_EN` (GPIO 2) is set HIGH in `earlyInitVariant()` to power the driver.

---

## What Was Removed

`hapticFeedback(false)` was called unconditionally in `TDeckProKeyboard::pressed()`,
causing a vibration pulse on every single keypress.

```cpp
// BEFORE — fired on every key
void TDeckProKeyboard::pressed(uint8_t key)
{
    ...
    hapticFeedback(false);   // ← removed
    playClick();
    ...
}
```

Removed the call. `playClick()` (audio click sound) is unaffected.

---

## What Remains

`hapticFeedback(bool special)` method still exists in `TDeckProKeyboard` for
future use. The `special` param distinguishes single-pulse (regular) vs
double-pulse (special action). Currently no call sites.

The DRV2605 state machine in `trigger()` (`_vibration_end`, `_haptic_phase`)
also remains — it would execute only if `hapticFeedback()` is called again.

---

## File Changed

| File | Change |
|------|--------|
| `src/input/TDeckProKeyboard.cpp` | Remove `hapticFeedback(false)` from `pressed()` |
