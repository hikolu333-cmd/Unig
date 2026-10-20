//
// DIY Joystick navigation — KY-023 dual-axis analog stick + SW button.
//
// VRx/VRy are read as 12-bit ADC values (0-4095) on ADC1 (safe alongside WiFi).
// Center is auto-calibrated in begin() by averaging a batch of readings with
// the stick assumed at rest, then each axis is compared against that center
// with a deadzone so small drift/noise around rest doesn't register a
// direction. Held past the deadzone repeats the direction every update(),
// same as a held digital button on the other boards.
//
// SW (active LOW) follows the same short-press/long-press pattern used by
// t_display/diy_smoochie: short press = DIR_PRESS, hold >600ms = DIR_BACK.
//
// Priority when the stick is diagonal: SW first, then Y axis (UP/DOWN), then
// X axis (LEFT/RIGHT) — only one Direction can be reported per frame.
//

#pragma once

#include "core/INavigation.h"

class NavigationImpl : public INavigation
{
public:
  void begin() override {
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    analogReadResolution(12);

    // Average a batch of readings at rest to find each axis' center. Run
    // after a short settle delay so the first ADC reads (which can be noisy
    // right after analogReadResolution()) don't skew the calibration.
    delay(20);
    uint32_t sumX = 0, sumY = 0;
    static constexpr uint8_t CAL_SAMPLES = 16;
    for (uint8_t i = 0; i < CAL_SAMPLES; i++) {
      sumX += analogRead(JOY_VRX_PIN);
      sumY += analogRead(JOY_VRY_PIN);
      delay(2);
    }
    _centerX = sumX / CAL_SAMPLES;
    _centerY = sumY / CAL_SAMPLES;
  }

  void update() override {
    uint32_t now = millis();

    // ── SW button: short press = PRESS, hold = BACK ─────────────────────
    bool swDown = (digitalRead(JOY_SW_PIN) == LOW);
    if (swDown) {
      if (!_swWasDown) {
        _swWasDown  = true;
        _swDownTime = now;
        _swLong     = false;
      }
      if (!_swLong && (now - _swDownTime) > LONG_PRESS_MS) {
        _swLong = true;
        updateState(DIR_BACK);
        return;
      }
      if (_swLong) { updateState(DIR_NONE); return; }
      updateState(DIR_PRESS);
      return;
    } else if (_swWasDown) {
      _swWasDown = false;
      _swLong    = false;
    }

    // ── Analog axes (deadzone around calibrated center) ─────────────────
    int32_t dx = (int32_t)analogRead(JOY_VRX_PIN) - (int32_t)_centerX;
    int32_t dy = (int32_t)analogRead(JOY_VRY_PIN) - (int32_t)_centerY;

    if (dy < -DEADZONE)      { updateState(DIR_UP);    return; }
    if (dy >  DEADZONE)      { updateState(DIR_DOWN);  return; }
    if (dx < -DEADZONE)      { updateState(DIR_LEFT);  return; }
    if (dx >  DEADZONE)      { updateState(DIR_RIGHT); return; }

    updateState(DIR_NONE);
  }

private:
  static constexpr uint32_t LONG_PRESS_MS = 600;
  // ~25% of the 12-bit range either side of center. KY-023 modules vary
  // module-to-module; widen this if idle jitter ever registers a direction,
  // narrow it if the stick feels unresponsive near center.
  static constexpr int32_t  DEADZONE      = 1000;

  uint16_t _centerX = 2048;
  uint16_t _centerY = 2048;

  uint32_t _swDownTime = 0;
  bool     _swWasDown  = false;
  bool     _swLong     = false;
};
