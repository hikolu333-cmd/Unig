//
// DIY RF Kit — TP4056 is a standalone charger IC (no I2C telemetry), MT3606
// is a dumb boost converter. Neither exposes battery % or charge status to
// the MCU, so getBatteryPercentage()/isCharging() stay stubbed.
// powerOff() uses deep sleep, waking on the joystick's SW button (active LOW).
//
// If you later swap TP4056 for a fuel-gauge/charger IC that talks I2C
// (MAX17048, BQ25896...), wire it on GROVE_SDA/GROVE_SCL and replace the two
// stub methods below — see boards/diy_smoochie/core/Power.h for a worked
// BQ25896 example.
//

#pragma once

#include "core/IPower.h"
#include "pins_arduino.h"
#include <Arduino.h>
#include <esp_sleep.h>

class PowerImpl : public IPower
{
public:
  void begin() override {}

  uint8_t getBatteryPercentage() override { return 0; }
  bool    isCharging()           override { return false; }

  void powerOff() override {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)JOY_SW_PIN, LOW);
    esp_deep_sleep_start();
  }
};
