# T-Deck Pro Voice

LILYGO T-Deck Pro variant with I2S audio output (PCM5102A DAC) and MEMS microphone, replacing
the 4G modem slot found on the standard T-Deck Pro. Meshtastic HW model 103 (`T_DECK_PRO_VOICE`).

---

## Hardware

| Component | Details |
|-----------|---------|
| SoC | ESP32-S3 |
| Display | E-Ink 3.1" — GxEPD2_310_GDEQ031T10 |
| Touch | CST328 capacitive (INT=GPIO 12, RST=GPIO 45) |
| Keyboard | TCA8418 matrix 4×10 (35 keys + 4 modifier keys), backlight GPIO 42 |
| LoRa | SX1262 / SX1268 |
| Audio out | PCM5102A I2S DAC (BCK=7, DOUT=8, WS=9, MCLK=N/A) |
| Microphone | I2S MEMS (CLK=18, DATA=17) |
| Vibration | GPIO 2 (simple on/off motor driver) |
| GPS | UART RX=44 TX=43 PPS=1 EN=15 @ 38400 baud |
| Battery fuel gauge | BQ27220 (1400 mAh design capacity) |
| Battery charger | BQ25896 (via PPM/XPOWERS) |
| Light sensor | LTR_553ALS |
| IMU | BHI260AP |
| SD card | SPI (CS=48, MOSI=33, SCK=36, MISO=47) |
| 1.8 V rail enable | GPIO 38 |
| BLE TX power | +18 dBm |

---

## Key Mappings

### Special / multi-tap keys

| Key | Modifier | Action |
|-----|----------|--------|
| `$` (SPEAKER key) | Shift | Read Aloud last received message |
| `$` (SPEAKER key) | Alt | Touch screen lock / unlock toggle |
| `p` | 5th tap (alt) | Send Ping |
| `g` | 5th tap (alt) | GPS Toggle |
| `m` | 5th tap (alt) | Mute / unmute notifications |
| `b` | 5th tap (alt) | Keyboard backlight toggle |
| `e` | 5th tap (alt) | Cursor Up |
| `s` | 5th tap (alt) | Cursor Left |
| `f` | 5th tap (alt) | Cursor Right |
| `x` | 5th tap (alt) | Cursor Down |
| `q` | 5th tap (alt) | ESC |
| `t` | 5th tap (alt) | Tab |

### Modifier keys

| Physical key | Function |
|---|---|
| Left Shift / Right Shift | Uppercase + special (tap 2) |
| Sym | Symbol layer (tap 3) |
| Alt | Action layer (tap 5) |

Multi-tap timeout: 1500 ms. Typing the same key within the window cycles through characters;
waiting past the timeout resets the cycle.

### PTT button (GPIO 0)

PTT button retains standard Meshtastic behavior (screen wake on short press, select on long press).
Codec2 voice TX is not wired — the hardware I2S path is output-only for this use case.

---

## Features vs T-Deck Pro (4G)

| Feature | T-Deck Pro (4G) | T-Deck Pro Voice |
|---------|-----------------|-----------------|
| Audio output | None | PCM5102A I2S DAC |
| Microphone | None | I2S MEMS |
| 4G modem | SIM7670G | Removed |
| Read Aloud (TTS) | No | Yes — Shift+`$` |
| RTTTL buzzer | No | Yes (via `use_i2s_as_buzzer`) |
| Codec2 voice TX | No | Disabled (hardware limits) |
| LoRa region default | UNSET | UNSET |

---

## Changes introduced on `tdeck-pro-voice-poc`

- **I2S audio:** Added `HAS_I2S`, `DAC_I2S_*` pin definitions; PCM5102A DAC on pins formerly used
  by 4G modem (BCK=7, DOUT=8, WS=9).
- **Microphone:** I2S MEMS mic on GPIO 17 (data) / 18 (clock).
- **AudioThread:** Active for TTS (`readAloud`) and RTTTL buzzer output.
- **Read Aloud shortcut:** Shift+`$` triggers `audioThread->readAloud()` on the last received
  text message. Implemented via `INPUT_BROKER_MSG_READ_ALOUD` (0xAE) in `SystemCommandsModule`.
- **Region default:** Removed hardcoded `REGION_US` override; falls through to `REGION_UNSET`
  like all other variants — user sets region on first boot.
- **Non-blocking haptic:** `TDeckProKeyboard::hapticFeedback()` converted from blocking `delay()`
  to millis-based state machine checked in `trigger()`. Regular key = 50 ms single pulse;
  special action key (Send Ping, GPS Toggle, Read Aloud) = 50 ms + 20 ms gap + 50 ms double pulse.
- **Non-blocking backlight blink:** Touch lock toggle backlight feedback moved to a short-lived
  FreeRTOS task so `handleInputEvent()` returns immediately.
- **Codec2 PTT disabled:** `moduleConfig.audio.ptt_pin` no longer set for this variant; codec2
  I2S config (bitrate, pins) remains for potential future receive-side use.
- **BLE, GPS, touch, E-Ink, keyboard backlight:** Unchanged from T-Deck Pro.
