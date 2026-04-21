# T-Deck Pro Voice - Audio & Vibration Debug Context

## CURRENT STATUS (2026-04-20)

### WORKING: Vibration (DRV2605 Haptic)
- Vibration working on keyboard key press (before session)
- DRV2605 chip init at boot

### NOT WORKING: Audio (I2S Buzzer)
- **Audio dead**
- Boot start melody: silent
- Read Aloud: silent
- No sound from PCM5102A DAC

## CHANGES MADE IN SESSION 2026-04-20

### 1. Vibration MOVED from Keyboard → Touchscreen + Message Received
- **REMOVED** vibration from keyboard keypress (`TDeckProKeyboard.cpp` line ~133-140)
- **ADDED** DRV2605 haptic to `TouchScreenBase::hapticFeedback()` for T_DECK_PRO
- **ADDED** DRV2605 haptic to `ExternalNotificationModule` when `vibraShouldAlert` true

### 2. Audio Bug Fixed: playTonesRTTTL return INSIDE loop
- **BUG**: `buzz.cpp`, `return` inside for loop (line 97)
- First tone play, function return immediately
- **FIX**: Moved `return` and `audioThread->beginRttl()` outside for loop
- All tones build into one RTTTL string, play once

### 3. Audio Gain Increased
- `AudioThread.h` line 96: `SetGain(0.2)` → `SetGain(0.5)` (2.5x louder)

### 4. I2S Port Changed
- `AudioThread.h` line 94: `AudioOutputI2S(1, ...)` → `AudioOutputI2S(0, ...)`

### 5. GPIO0 Conflict Fixed (MOST CRITICAL)
- **PROBLEM**: `variant.h` had `DAC_I2S_MCLK 0` conflict with `BUTTON_PIN 0`
- **FIX**: Changed to `DAC_I2S_MCLK -1` (unused) — PCM5102A has internal PLL
- PCM5102A no need MCLK — only BCK, DOUT, WS required

### 6. GPIO0 in stopNow() Fixed
- `ExternalNotificationModule.cpp` line ~270: Added `!defined(T_DECK_PRO)` exclusion
- T-Deck Pro Voice GPIO0 must NOT restore to INPUT after audio stop

### 7. Debug Logging Added
- `AudioThread.h`: Logs `AudioThread I2S init: BCK=%d WS=%d DOUT=%d MCLK=%d result=%d`
- `AudioThread.h`: Logs `AudioThread beginRttl: len=%u`
- `buzz.cpp`: Logs `playTonesRTTTL: size=%d rtttl='%s'`

## FILES MODIFIED

| File | Changes |
|------|---------|
| `src/input/TDeckProKeyboard.cpp` | Removed hapticFeedback() call from pressed() |
| `src/input/TouchScreenBase.cpp` | Added T_DECK_PRO to hapticFeedback() |
| `src/modules/ExternalNotificationModule.cpp` | Added DRV2605 in vibraShouldAlert block, fixed GPIO0 restore |
| `src/AudioThread.h` | Gain 0.2→0.5, port 1→0, added logging |
| `src/buzz/buzz.cpp` | Fixed return outside loop, added logging |
| `variants/esp32s3/t-deck-pro-voice/variant.h` | MCLK 0 → -1 |

## DEVICE CONNECTION
- Port: `/dev/ttyACM0` at 115200 baud
- Build: `pio run -e t-deck-pro-voice`
- Flash: `pio run -e t-deck-pro-voice -t upload --upload-port /dev/ttyACM0`

## FIRMWARE
- Version: 2.7.23.d562a07
- Binary: `.pio/build/t-deck-pro-voice/firmware-t-deck-pro-voice-2.7.23.d562a07.factory.bin`

## USER TESTING CHECKLIST
- [ ] Touchscreen touch → vibrate (DRV2605)
- [ ] Message received → vibrate (if unmuted)
- [ ] Keyboard → NOT vibrate
- [ ] Boot sound → play ALL notes, louder
- [ ] Serial logs show `playTonesRTTTL` and `AudioThread beginRttl`

## ORACLE FINDINGS
1. **GPIO0 conflict** — BUTTON_PIN and MCLK both use GPIO0 — FIXED
2. **PCM5102A no need MCLK** — internal PLL — FIXED
3. **playTonesRTTTL bug** — return inside loop — FIXED
4. **Gain too low** — 0.2 → 0.5 — FIXED

## NEXT STEPS IF AUDIO STILL NOT WORKING
1. Verify I2S pins (BCK=7, WS=9, DOUT=8) correct in hardware
2. Check PCM5102A XMT pin HIGH (enable output)
3. Add oscilloscope/logic analyzer, verify I2S signals
4. Try different I2S sample rate
5. Check ESP8266Audio library version correct