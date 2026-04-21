# T-Deck Pro Voice — Audio Debug Session

## Status: RESOLVED (2026-04-21)

Audio fully working. Boot melody plays. RTTTL tones on message receive.

---

## Root Causes Found (all three required fixes)

### 1. MCLK = -1 rejected by ESP32-S3 I2S driver

The ESP32-S3 legacy I2S driver (`i2s_check_set_mclk`) does not accept `-1` as `I2S_PIN_NO_CHANGE`. Boot log error:

```
E (3119) I2S: i2s_check_set_mclk(266): mck_io_num invalid
E (3119) I2S: i2s_set_pin(314): mclk config failed
```

PCM5102A does not need MCLK (internal PLL), but the ESP32-S3 driver still requires a valid GPIO number. **Fix: `DAC_I2S_MCLK 21`** — valid spare GPIO, already set OUTPUT HIGH in `earlyInitVariant()`.

Values that fail: `-1`, `39`. Use `21` (or any other valid unused GPIO).

### 2. audioThread created after playStartMelody()

In `src/main.cpp`, `audioThread = new AudioThread()` was located ~150 lines after `playStartMelody()`. Since `playStartMelody()` checks `audioThread != nullptr`, the boot melody was silently skipped every time.

**Fix:** moved `audioThread` creation to immediately before `playStartMelody()`.

### 3. SetGain(4.0) = silence (uint8_t overflow)

`AudioOutput::SetGain(f)` stores: `gainF2P6 = (uint8_t)(f * 64)`.

`SetGain(4.0)` → `(uint8_t)(256)` → `0` → all samples multiplied by zero → silence.

**Fix:** `SetGain(3.0)` — loud without overflow. Max safe value is `3.984` (`255/64`).

---

## What Was Tried (chronological)

| Attempt | Result |
|---------|--------|
| `MCLK = -1` (original POC) | Fails — ESP32-S3 driver rejects -1 |
| `MCLK = 39` (other agent) | Fails — ESP32-S3 driver also rejects 39 |
| `MCLK = 21` | Works — valid GPIO accepted by driver |
| `SetGain(1.0)` | Audio confirmed working at acceptable volume |
| `SetGain(4.0)` | Silent — uint8_t overflow to 0 |
| `SetGain(3.0)` | Working — good volume |

---

## Library Notes

Meshtastic uses `earlephilhower/ESP8266Audio@1.9.9`. Lilygo's official demos use `schreibfaul1/ESP32-audioI2S`. Both work with PCM5102A once pins and MCLK are correct.

The library switch hypothesis (from earlier session) was **incorrect** — the actual blockers were MCLK pin and audioThread ordering, not a library incompatibility.

---

## Hardware Confirmed

| Item | Detail |
|------|--------|
| DAC | PCM5102A, BCK=7, DOUT=8, WS=9 |
| MCLK | GPIO 21 (dummy output — PCM5102A uses internal PLL) |
| Power enable | GPIO 41 HIGH (confirmed from Lilygo demo `BOARD_6609_EN`) |
| MCU | ESP32-S3 |
| I2S port | Port 1 (`AudioOutputI2S(1, EXTERNAL_I2S)`) |

---

## Commits

| Hash | Description |
|------|-------------|
| `0bd3922` | Fix MCLK=21, audioThread ordering, gain 1.0 — audio working |
| `740041d` | Fix gain 3.0 (4.0 overflows uint8_t = silence) |
