//
// DIY Joystick — no battery/charger IC assumed (bare ESP32-S3 devkit).
// getBatteryPercentage()/isCharging() report nothing; powerOff() uses deep
// sleep, waking on the joystick's SW button (active LOW).
//
// If you add a fuel-gauge/charger IC (e.g. MAX17048, BQ25896) later, wire it
// on GROVE_SDA/GROVE_SCL and replace the two stub methods below — see
// boards/diy_smoochie/core/Power.h for a worked BQ25896 example.
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
