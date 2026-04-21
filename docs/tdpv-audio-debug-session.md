# T-Deck Pro Voice — Audio Debug Session Notes

## Problem
T-Deck Pro Voice no audio output for boot melody + RTTTL buzzer despite:
- PCM5102A DAC present on I2S pins (BCK=7, DOUT=8, WS=9)
- `HAS_I2S` defined in variant
- `use_i2s_as_buzzer=true` in module config
- Haptic (DRV2605) work fine — same `playTones()` code path
- Official Meshtastic `AudioThread.h` code (port=1, gain=0.2, no explicit `begin()`)

## Hardware
| Component | Detail |
|-----------|--------|
| DAC | PCM5102A I2S (BCK=7, DOUT=8, WS=9, MCLK=-1) |
| MCU | ESP32-S3 |
| Audio lib | `earlephilhower/ESP8266Audio@1.9.9` |
| Variant | `esp32s3/t-deck-pro-voice` |

## What Was Tried

### LOG_ERROR Debugging (VERIFIED)
Binary flashed with LOG_ERROR confirm:
- `AudioThread: ctor ENTRY` — constructor called ✅
- `AudioThread initOutput: BCK=7 WS=9 DOUT=8 MCLK=-1 pinResult=1 beginResult=1` — SetPinout SUCCESS ✅, begin SUCCESS ✅
- `About to call playStartMelody` — reached ✅
- `AudioThread beginRttl: len=34` — called with 34 chars ✅

All init succeed. Audio no play.

### AudioOutputI2S begin() Call
Tried explicit `audioOut->begin()` in `initOutput()` — no change.

### Pin Assignment
Verified against Lilygo T-Deck Pro repo:
- BCK=7 ✅
- WS=9 ✅
- DOUT=8 ✅
- MCLK=-1 ✅

## Key Finding: Library Mismatch

**Lilygo official T-Deck Pro demo use `esphome/ESP32-audioI2S`** (based on `schreibfaul1/ESP32-audioI2S`), NOT `ESP8266Audio`.

| Library | Used by | Works with PCM5102A |
|---------|---------|-------------------|
| `ESP8266Audio` (Earle F. Philhower) | Meshtastic | Supposedly yes |
| `esphome/ESP32-audioI2S` | Lilygo official demo | Yes — confirmed working in Lilygo demo |

Lilygo demo code (`examples/test_pcm5102a/test_pcm5102a.ino`):
```cpp
Audio audio;
audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
audio.setVolume(21);
audio.connecttoFS(SPIFFS, "/iphone_call.mp3");
```

## Root Cause Hypothesis

`ESP8266Audio` `AudioOutputI2S` I2S timing/init sequence differ from PCM5102A DAC expect. `esphome/ESP32-audioI2S` apparently has board-specific init that work.

## Recommended Fix

1. **Switch from `ESP8266Audio` to `esphome/ESP32-audioI2S`** library
2. Restructure `AudioThread` to use esphome `Audio` class
3. Use `Audio::connecttoFS()` or `Audio::connecttohost()` for playback
4. Alternative: keep `ESP8266Audio` but port esphome PCM5102A-specific init sequence

## Files Modified (Session)

| File | Change |
|------|--------|
| `src/AudioThread.h` | Reverted to upstream (port=1, gain=0.2, no begin()) |
| `src/main.cpp` | Moved `audioThread = new AudioThread()` before `playStartMelody()` |
| `src/buzz/buzz.cpp` | Reordered checks so HAS_I2S path checked before buzzer_mode |

## Commands Used
```bash
# Build
pio run -e t-deck-pro-voice

# Flash (no-reset, after killing port holder)
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --port /dev/ttyACM0 --chip esp32s3 --baud 460800 --after no_reset write_flash 0x10000 .pio/build/t-deck-pro-voice/firmware-t-deck-pro-voice-2.7.23.d562a07.bin

# Reboot from bootloader
esptool.py --after hard_reset run

# Capture boot serial
python3 -c "import serial,time; s=serial.Serial('/dev/ttyACM0',115200,timeout=0.05); ..."
```

## References
- Lilygo T-Deck Pro repo: https://github.com/Xinyuan-LilyGO/T-Deck-Pro
- esphome/ESP32-audioI2S: https://github.com/schreibfaul1/ESP32-audioI2S
- PCM5102A pinout: BCK=7, DOUT=8, WS=9, MCLK=-1 (from variant.h)