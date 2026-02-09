/*
 * T-Deck Pro Voice Variant
 *
 * Hardware variant of the LilyGo T-Deck Pro with PCM5102A I2S DAC
 * for speaker output and vibration motor for haptic feedback.
 * This variant does NOT have the cellular modem; GPIO 7/8/9 are
 * repurposed for the I2S speaker interface.
 */

#ifndef _VARIANT_TDECK_PRO_VOICE_H_
#define _VARIANT_TDECK_PRO_VOICE_H_

// Display (E-Ink) - same as T-Deck Pro
#define PIN_EINK_CS 34
#define PIN_EINK_BUSY 37
#define PIN_EINK_DC 35
#define PIN_EINK_RES -1
#define PIN_EINK_SCLK 36
#define PIN_EINK_MOSI 47

#define I2C_SDA SDA
#define I2C_SCL SCL

// CST328 touch screen
#define HAS_TOUCHSCREEN 1
#define CST328_PIN_INT 12
#define CST328_PIN_RST 45

#define USE_POWERSAVE
#define SLEEP_TIME 120

// GNSS
#define HAS_GPS 1
#define GPS_BAUDRATE 38400
#define PIN_GPS_EN 15
#define GPS_EN_ACTIVE 1
#define GPS_RX_PIN 44
#define GPS_TX_PIN 43
#define PIN_GPS_PPS 1

#define BUTTON_PIN 0

// Vibration motor
#define PIN_VIBRATION 2
#define HAS_VIBRATION_MOTOR

// Have SPI interface SD card slot
#define HAS_SDCARD
#define SDCARD_USE_SPI1
#define SPI_MOSI (33)
#define SPI_SCK (36)
#define SPI_MISO (47)
#define SPI_CS (48)
#define SDCARD_CS SPI_CS
#define SD_SPI_FREQUENCY 75000000U

// TCA8418 keyboard
#define KB_BL_PIN 42

// I2S Speaker (PCM5102A DAC) - Voice variant specific
// These GPIOs are used for the modem on the standard T-Deck Pro,
// but repurposed for the PCM5102A I2S DAC on the Voice variant.
#define HAS_I2S
#define HAS_I2S_SPEAKER
#define DAC_I2S_BCK 7
#define DAC_I2S_DOUT 8
#define DAC_I2S_WS 9
#define DAC_I2S_MCLK -1 // PCM5102A has internal PLL, no MCLK needed

// I2S speaker pin aliases for the I2SSpeaker driver
#define I2S_SPEAKER_BCLK DAC_I2S_BCK
#define I2S_SPEAKER_DOUT DAC_I2S_DOUT
#define I2S_SPEAKER_LRC DAC_I2S_WS

// Audio configuration
#define AUDIO_SAMPLE_RATE 16000

// Microphone (future use)
#define PCM5102A_MIC_DATA 17
#define PCM5102A_MIC_CLOCK 18

// LTR_553ALS light sensor
#define HAS_LTR553ALS

// Gyroscope BHI260AP
#define BOARD_1V8_EN 38
#define HAS_BHI260AP

// Battery charger BQ25896
#define HAS_PPM 1
#define XPOWERS_CHIP_BQ25896

// Battery gauge BQ27220
#define HAS_BQ27220 1
#define BQ27220_I2C_SDA SDA
#define BQ27220_I2C_SCL SCL
#define BQ27220_DESIGN_CAPACITY 1400

// LoRa
#define USE_SX1262
#define USE_SX1268

#define LORA_EN 46
#define LORA_SCK 36
#define LORA_MISO 47
#define LORA_MOSI 33
#define LORA_CS 3

#define LORA_DIO0 -1
#define LORA_RESET 4
#define LORA_DIO1 5
#define LORA_DIO2 6
#define LORA_DIO3

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_DIO2
#define SX126X_RESET LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 2.4

#define HAS_PHYSICAL_KEYBOARD 1

#endif // _VARIANT_TDECK_PRO_VOICE_H_
