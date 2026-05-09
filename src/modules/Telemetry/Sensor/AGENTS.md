# src/modules/Telemetry/Sensor — 50+ I2C Sensor Drivers

**85 files.** Sensors auto-detected at boot via `src/detect/ScanI2C`. Each sensor implements `initDevice()`, `runOnce()`, `getMetrics()`.

## Structure

```
Sensor/
├── TelemetrySensor.h/.cpp      # Base class
├── AddI2CSensorTemplate.h      # Template auto-registration
├── sensors/                    # Individual drivers
│   ├── Power/:  INA219, INA226, INA260, INA3221, MAX17048
│   ├── Env/:    BME280, BME680, BMP280, BMP3XX, DPS310, LPS22HB, SCD4X, SEN5X
│   ├── Light/:  BH1750, TSL2561, TSL2591, VEML7700, LTR390UV, OPT3001
│   └── Special/: CGRadSens, NAU7802, MAX30102, MLX90614, MLX90632
└── config/                     # BSEC2 IAQ configs for BME680
```

## Sensor Lifecycle

```cpp
class MySensor : public TelemetrySensor {
    initDevice(device) → setup I2C → initI2CSensor() [AT END]
    runOnce() → periodic polling → returns next interval ms
    getMetrics(measurement) → populate protobuf variant union
};
```

## Key Conventions

1. **`initI2CSensor()` called at END of `initDevice()`** — registers sensor in global `nodeTelemetrySensorsMap[]`
2. **Mixin pattern**: `INA219Sensor : public TelemetrySensor, VoltageSensor, CurrentSensor`
3. **Dual-protobuf routing**: `INA3221Sensor::getMetrics()` switches on `which_variant` tag
4. **Singleton for single-instance hardware**: `MAX17048Singleton`
5. **State machines for air quality**: SCD4X (`SCD4X_OFF/IDLE/MEASUREMENT`), SEN5X (5-state)
6. **I2C reclocking**: `reClockI2C(SPEED, bus, restoreOnExit)` in SCD4X, SEN5X

## Air Quality Sensors (Complex)

| Sensor | State Machine | Warm-up | Special                          |
| ------ | ------------- | ------- | -------------------------------- |
| SCD4X  | 3-state       | 5s      | Clock reclocking                 |
| SEN5X  | 5-state       | 30s     | `/prefs/sen5X.dat` persistence   |
| BME680 | +BSEC2 IAQ    | ~4h     | BSEC2 library, `/prefs/bsec.dat` |
| SCD30  | simple        | 2s      | —                                |

## Adding a New Sensor

1. Create driver in `src/modules/Telemetry/Sensor/sensors/<Category>/`
2. Register I2C address in `src/detect/ScanI2C::addGenericI2CDevices()`
3. Call `initI2CSensor()` at end of `initDevice()`
4. Add proto fields in `protobufs/meshtastic/telemetry.proto`

## BSEC2 IAQ (BME680)

```cpp
#if __has_include(<bsec2.h>)
    Bsec2 bme680;
    #include "config/bme680/bme680_iaq_33v_3s_4d/bsec_iaq.txt"
#endif
```

## Template Registration

```cpp
#include "AddI2CSensorTemplate.h"
addSensor<MySensor>(scanner, ScanI2C::DeviceType::MY_SENSOR);
```
