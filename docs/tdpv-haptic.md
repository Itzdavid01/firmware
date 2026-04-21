# T-Deck Pro Voice — Haptic Feedback

## Status
- Keypress haptic: REMOVED (2026-04-21)
- Touchscreen haptic: shortened to short click (2026-04-21)

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

---

## Touchscreen Haptic — Shortened (2026-04-21)

`TouchScreenBase::hapticFeedback()` in `src/input/TouchScreenBase.cpp` used
DRV2605 waveform 16 (Long Buzzer 100%, ~1 second) for touch events.
This held the motor for ~1s per tap, making the UI feel sluggish.

Changed to waveform 1 (Strong Click 100%, ~5ms):

```cpp
// BEFORE
drv.setWaveform(0, 16); // Long Buzzer 100% (~1s)

// AFTER
drv.setWaveform(0, 1);  // Strong Click 100% (~5ms)
```

The guard `#if defined(T_WATCH_S3) || defined(T_DECK_PRO)` means this
affects both T-Watch S3 and all T-Deck Pro variants.

### DRV2605 Waveform Reference (Library 1)

| # | Name | Duration |
|---|------|---------|
| 1 | Strong Click 100% | ~5ms ← **touch feedback** |
| 3 | Strong Click 30% | ~5ms (softer) |
| 7 | Sharp Tick 100% | ~2ms (shortest click) |
| 14 | Strong Buzz 100% | ~300ms |
| 16 | Long Buzzer 100% | ~1000ms ← was used |

If still too strong, try waveform 3 (30%) or 7 (Sharp Tick).

---

## Files Changed

| File | Change |
|------|--------|
| `src/input/TDeckProKeyboard.cpp` | Remove `hapticFeedback(false)` from `pressed()` |
| `src/input/TouchScreenBase.cpp` | Waveform 16 → 1 for touch events |
