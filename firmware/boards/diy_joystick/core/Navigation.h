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
#include "core/Device.h"

class NavigationImpl : public INavigation
{
public:
  // Track orientation for the touch-zone flip (base member is private).
  void setRightHand(bool v) override { INavigation::setRightHand(v); _touchFlip = v; }

  void begin() override {
    pinMode(JOY_SW_PIN, INPUT_PULLUP);

#ifdef JOYSTICK_NOT_WIRED
    // Stick unplanned for now: floating VRx/VRy would produce random
    // directions and wake the display from power-save. Freeze all input.
    return;
#else
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
#endif

    // XPT2046 touch (TOUCH_CS=41, shared display SPI) — same calData shape
    // as the CYD W328R backend: {xMin, xMax, yMin, yMax, rotation}.
    static uint16_t calData[5] = { TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX,
                                   TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX, 1 };
    Uni.Lcd.setTouch(calData);
  }

  void update() override {
#ifdef JOYSTICK_NOT_WIRED
    updateState(DIR_NONE);
    return;
#else
    uint32_t now = millis();

    // ── Touch (XPT2046) — polled every 20 ms, overrides the stick while a
    // finger is down. Zones mirror the CYD layout: left quarter = BACK,
    // right 3/4 split in thirds = UP / PRESS / DOWN.
    if (now - _touchPoll >= 20 && false) {   // TEMP OFF: XPT2046 dead on this board — poll disabled until the module is replaced
      _touchPoll = now;
      uint16_t tx, ty;
      if (Uni.Lcd.getTouch(&tx, &ty)) {
        constexpr int16_t SW_ = TFT_HEIGHT, SH_ = TFT_WIDTH;   // landscape
        if (_touchFlip) { tx = SW_ - 1 - tx; ty = SH_ - 1 - ty; }
        if (tx >= SW_) tx = SW_ - 1;
        if (ty >= SH_) ty = SH_ - 1;
        Direction d;
        if (tx < SW_ / 4)                      d = DIR_BACK;
        else if (ty < SH_ / 3)                 d = DIR_UP;
        else if (ty < (SH_ / 3) * 2)           d = DIR_PRESS;
        else                                   d = DIR_DOWN;
        _touchCnt = 0;
        updateState(d);
        return;
      }
      if (++_touchCnt < 3) { updateState(DIR_NONE); return; }   // debounce release
      _touchCnt = 3;
    }

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
#endif
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

  uint32_t _touchPoll = 0;
  uint8_t  _touchCnt  = 3;
  bool     _touchFlip = false;
};
