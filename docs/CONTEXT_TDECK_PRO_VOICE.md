# T-Deck Pro Voice — Audio & Vibration Debug Context

## Problem Statement
T-Deck Pro Voice audio AND vibration dead. Neither work.

## Hardware Mapping (LILYGO verified)
- Audio I2S: GPIO 7 (BCK), GPIO 8 (DOUT), GPIO 9 (WS) — PCM5102A DAC
- Vibration motor: GPIO 2 (simple on/off driver)
- 4G modem pins (7,8,9 standard) removed on Voice variant — no conflict

## Code Changes Applied

### 1. `variants/esp32s3/t-deck-pro-voice/variant.cpp`
```cpp
void earlyInitVariant()
{
    pinMode(LORA_EN, OUTPUT);
    digitalWrite(LORA_EN, HIGH);
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(PIN_EINK_CS, OUTPUT);
    digitalWrite(PIN_EINK_CS, HIGH);
    pinMode(PIN_VIBRATION, OUTPUT);
    digitalWrite(PIN_VIBRATION, LOW); // ensure motor is off initially
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    bool ok = ledcAttach(PIN_VIBRATION, VIBRATION_PWM_FREQ, VIBRATION_PWM_RES);
    LOG_DEBUG("Vibration LEDC init: pin=%d, freq=%d, bits=%d, result=%d", PIN_VIBRATION, VIBRATION_PWM_FREQ, VIBRATION_PWM_RES, ok);
    if (!ok) {
        LOG_ERROR("LEDC attach failed for vibration pin %d", PIN_VIBRATION);
    }
#else
    ledcSetup(VIBRATION_PWM_CHANNEL, VIBRATION_PWM_FREQ, VIBRATION_PWM_RES);
    ledcAttachPin(PIN_VIBRATION, VIBRATION_PWM_CHANNEL);
#endif
}
```

### 2. `variants/esp32s3/t-deck-pro-voice/variant.h`
```cpp
// vibration motor
#define PIN_VIBRATION 2
#define VIBRATION_PWM_CHANNEL 5
#define VIBRATION_PWM_FREQ 5000
#define VIBRATION_PWM_RES 8
```

### 3. `src/input/TDeckProKeyboard.h` — Added state variables
```cpp
uint8_t _vibration_target = 0;
uint8_t _vibration_ramp = 0;
uint8_t _haptic_phase = 0;
uint32_t _vibration_end = 0;
```

### 4. `src/input/TDeckProKeyboard.cpp` — Vibration state machine
Replaced `digitalWrite(PIN_VIBRATION, on)` with `ledcWrite(PIN_VIBRATION, duty)` + soft-start ramp. Fixed `_haptic_phase = special ? 1 : 0` bug (regular keys got phase=0 → motor OFF instant).

### 5. `src/AudioThread.h` — Gain increased
```cpp
audioOut->SetGain(1.0);  // was 0.2, 5x louder
```

### 6. `src/mesh/NodeDB.cpp` — I2S buzzer ENABLED for T_DECK_PRO_VOICE
```cpp
// BEFORE: T_DECK_PRO_VOICE was excluded from I2S buzzer
#ifdef HAS_I2S
#ifndef T_DECK_PRO_VOICE
    moduleConfig.external_notification.use_i2s_as_buzzer = true;
    ...
#endif
#endif

// AFTER: T_DECK_PRO_VOICE now uses I2S for buzzer notifications
#ifdef HAS_I2S
    moduleConfig.external_notification.enabled = true;
    moduleConfig.external_notification.use_i2s_as_buzzer = true;
    moduleConfig.external_notification.alert_message_buzzer = true;
    ...
#endif
```

## Build Output
```
~/firmware-builds/t-deck-pro-voice/
├── firmware-t-deck-pro-voice-2.7.23.d562a07.bin (2.1 MB)
└── firmware-t-deck-pro-voice-2.7.23.d562a07.factory.bin (2.2 MB)
```

## Key Debug Logs to Look For
1. Boot: `Vibration LEDC init: pin=2, freq=5000, bits=8, result=X` (1=success, 0=fail)
2. Boot: `AudioOutputI2S init: SetPinout=X, gain=1.0` (1=success, 0=fail)
3. Boot: `AudioThread ctor: audioOut=0x...`
4. Keypress: `hapticFeedback: special=X, target=XXX`
5. Keypress: `VIB phase0 off, duty=0` or `VIB phase1 on, duty=XXX`

## Status
- Build: COMPLETE
- Flash: PENDING
- Serial log capture: PENDING
- Audio verification: PENDING
- Vibration verification: PENDING

## Issue Hypothesis
User saw something in logs → code runs. But no audio, no vibration. Possible causes:
1. `ledcWrite(PIN_VIBRATION, ...)` broken on ESP-IDF 5.x (may need channel-based API)
2. `AudioOutputI2S` `SetPinout()` returns false (I2S GPIO matrix routing fails)
3. AudioThread not created/started
4. `buzzer_mode` config blocks audio

## Next Steps (for next session if interrupted)
1. Flash firmware to /dev/ttyACM0
2. Open serial monitor, capture full boot + keypress logs
3. Check `AudioThread ctor` log appears (proves AudioThread created)
4. Check `Vibration LEDC init: result=1` appears (proves LEDC init success)
5. Both appear but no audio/vibration → likely API mismatch (ledcWrite vs ledcWrite tone)