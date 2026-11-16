//
// RadioKeeper — on-demand radio with weak-rail protection (diy_joystick).
//
// The radio powers up ONLY when a WiFi/BT feature actually needs it and
// powers back down when the app releases it — no boot-time warm-up, no
// keep-alive: idle boards stay cool. (The boot-time warm-up + "radio never
// powers down" strategy was a workaround for one dead devkit and is retired;
// see git history / release v1.9.x notes.)
//
// For budget ESP32-S3 devkit clones with a collapsing 3V3 rail (weak LDO /
// missing bulk capacitance) the modem power-up spike inside esp_wifi_start()
// browns the board out (reset reason POWERON/BROWNOUT — a hardware
// signature). That protection is kept, as pure insurance, at zero cost for
// healthy boards:
//  1. Every esp_wifi_start() is wrapped: an NVS "attempting" flag is set
//     before the real call, so a brownout mid-start is recognized by the
//     NEXT boot (attempting + POWERON/BROWNOUT) and counted.
//  2. After 5 counted failures per power cycle, starts are refused with
//     ESP_ERR_WIFI_STATE — WiFi screens report an error instead of
//     reboot-looping the board (graceful degradation).
//  3. Once a weak rail is detected (any counted failure), the RF
//     calibration inside PHY init is downgraded to PHY_RF_CAL_NONE (the
//     deep-sleep wake path) to dodge the PA calibration spike. Healthy
//     boards always get the full calibration for accurate TX.
//  4. Telemetry goes to both consoles (native USB + UART0): the UART
//     bridge survives radio power-up, the native USB console usually
//     does not.
//
// Service key 'r' (either serial console) resets the weak-rail state for another try.
//

#pragma once

#include <esp_wifi.h>

class RadioKeeper
{
public:
  // Call once early in setup() (no radio activity, no heat): classifies the
  // previous reset, counts mid-start brownouts, decides the gave-up state.
  static void bootCheck();

  // True when starts are refused this power cycle (rail can't power the
  // modem). WiFi entry points then fail gracefully instead of browning the
  // board out.
  static bool gaveUp() { return s_gaveUp; }

  // True once a weak rail has been detected (sticky; cleared by resetFails).
  static bool railWeak() { return s_railWeak; }

  // Service-key 'r': clear the failure counters and the sticky weak-rail
  // flag — the next WiFi screen retries the radio from scratch.
  static void resetFails();

private:
  static bool s_gaveUp;
  static bool s_railWeak;
};
