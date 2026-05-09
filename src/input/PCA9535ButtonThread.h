#pragma once

#include "InputBroker.h"
#include "Observer.h"
#include "concurrency/OSThread.h"
#include <Wire.h>

/**
 * @brief Thread to poll the PCA9535 I2C GPIO expander for button presses.
 * Specific to LilyGo T5S3 E-Paper Pro.
 */
class PCA9535ButtonThread : public Observable<const InputEvent *>, public concurrency::OSThread
{
  public:
    PCA9535ButtonThread();
    void setBacklight(bool on);

  protected:
    virtual int32_t runOnce() override;

  private:
    uint8_t lastPort1State = 0xFF;
    uint32_t lastDebounceTime = 0;
    static constexpr uint8_t PCA9535_ADDR = 0x20;
    static constexpr uint8_t REG_INPUT_PORT0 = 0x00;
    static constexpr uint8_t REG_INPUT_PORT1 = 0x01;
    static constexpr uint8_t REG_OUTPUT_PORT0 = 0x02;
    static constexpr uint8_t REG_OUTPUT_PORT1 = 0x03;
    static constexpr uint8_t REG_CONFIG_PORT0 = 0x06;
    static constexpr uint8_t REG_CONFIG_PORT1 = 0x07;
};

extern PCA9535ButtonThread *pca9535Buttons;
