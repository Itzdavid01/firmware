# T-Deck Pro Voice — Audio Configuration

## Status: WORKING (as of 2026-04-21)

Boot melody plays on device startup. RTTTL tones play on message receive.

---

## Audio Hardware

T-Deck Pro Voice uses **PCM5102A** I2S DAC.

| Signal | GPIO | Notes |
|--------|------|-------|
| BCK (Bit Clock) | 7 | I2S bit clock |
| DOUT (Data Out) | 8 | I2S serial data |
| WS (Word Select) | 9 | I2S left/right clock |
| MCLK | 21 | Dummy — PCM5102A has internal PLL; valid GPIO required by ESP32-S3 driver |

Defined in `variants/esp32s3/t-deck-pro-voice/variant.h`:

```cpp
#define HAS_I2S
#define DAC_I2S_BCK 7
#define DAC_I2S_DOUT 8
#define DAC_I2S_WS 9
#define DAC_I2S_MCLK 21  // ESP32-S3 I2S driver requires valid GPIO even if PCM5102A ignores MCLK
```

### Why MCLK = 21

PCM5102A derives all clocks internally from BCK via PLL — it does not need an external MCLK signal. However, the ESP32-S3 legacy I2S driver (`i2s_check_set_mclk`) rejects `-1` (I2S_PIN_NO_CHANGE) as invalid and throws:

```
E (3119) I2S: i2s_check_set_mclk(266): mck_io_num invalid
E (3119) I2S: i2s_set_pin(314): mclk config failed
```

GPIO 21 is used as the MCLK output. The PCM5102A ignores whatever signal appears on it. GPIO 21 is also set OUTPUT HIGH in `earlyInitVariant()`.

**Do not use -1, 0, or 39:**
- `-1` → rejected by ESP32-S3 I2S driver (silent failure / error)
- `0` → conflicts with `BUTTON_PIN 0`
- `39` → also rejected by ESP32-S3 I2S driver

---

## AudioThread Configuration

`src/AudioThread.h`:

```cpp
void initOutput()
{
    audioOut = std::unique_ptr<AudioOutputI2S>(new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S));
    audioOut->SetPinout(DAC_I2S_BCK, DAC_I2S_WS, DAC_I2S_DOUT, DAC_I2S_MCLK);
    audioOut->SetGain(3.0);
};
```

### SetGain — Critical Notes

`AudioOutput::SetGain(f)` stores gain as `uint8_t gainF2P6 = (uint8_t)(f * 64)`.

**`SetGain(4.0)` causes complete silence** — `4.0 * 64 = 256` overflows `uint8_t` to `0`, muting all output.

| Gain value | gainF2P6 | Result |
|-----------|---------|--------|
| 0.2 | 12 | Upstream default — very quiet |
| 1.0 | 64 | Audible |
| 3.0 | 192 | Good volume — current setting |
| 3.984 | 255 | Maximum without overflow |
| 4.0 | 0 (overflow) | **Silence** |

Safe maximum: `3.984` (`255/64`). Use `3.0` for headroom.

### audioThread Must Be Created Before playStartMelody()

`src/main.cpp` creates `audioThread` immediately before calling `playStartMelody()`. If `audioThread` is null when `playStartMelody()` runs, the boot melody is silently skipped.

```cpp
router = new ReliableRouter();

#ifdef HAS_I2S
    LOG_DEBUG("Start audio thread");
    audioThread = new AudioThread();
#endif

    // only play start melody when role is not tracker or sensor
    if (...)
        playStartMelody();
```

---

## Power Enable

GPIO 41 (`BOARD_6609_EN`) must be HIGH for the audio circuit to work. Set in `earlyInitVariant()` in `variants/esp32s3/t-deck-pro-voice/variant.cpp`:

```cpp
pinMode(41, OUTPUT); digitalWrite(41, HIGH);  // Audio circuit power enable
```

This is confirmed by Lilygo's official PCM5102A demo (`examples/test_pcm5102a/test_pcm5102a.ino`).

---

## Config: use_i2s_as_buzzer

`NodeDB::loadFromDisk()` forces `use_i2s_as_buzzer = true` and `buzzer_mode = ALL_ENABLED` for `T_DECK_PRO_VOICE` after loading saved config, so saved settings can't accidentally disable I2S audio.

---

## Files Changed

| File | Change |
|------|--------|
| `variants/esp32s3/t-deck-pro-voice/variant.h` | `DAC_I2S_MCLK 21` (not -1, 0, or 39) |
| `variants/esp32s3/t-deck-pro-voice/variant.cpp` | GPIO 41 HIGH in `earlyInitVariant()` |
| `src/AudioThread.h` | Port 1, `SetGain(3.0)` |
| `src/main.cpp` | `audioThread` created before `playStartMelody()` |
| `src/buzz/buzz.cpp` | `HAS_I2S` check before `buzzer_mode` check |
| `src/mesh/NodeDB.cpp` | Force `use_i2s_as_buzzer=true` post config load |
