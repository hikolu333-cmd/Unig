//
// RadioKeeper — radio bring-up keeper for the diy_joystick board.
//
// Budget ESP32-S3 devkit clones often have a 3V3 rail (weak LDO / missing
// bulk capacitance) that collapses under the current spike of the modem
// power-up inside esp_wifi_start(): the board browns out and reboots (reset
// reason POWERON/BROWNOUT — a hardware signature, not a crash).
//
// Strategy (all software, no hardware changes required):
//  1. Warm the radio up ONCE per power cycle at boot, before the display
//     and LVGL load the rail: CPU clocked down, panel dark, RF calibration
//     downgraded to PHY_RF_CAL_NONE (the deep-sleep wake path) to dodge the
//     PA calibration spike.
//  2. Retry up to 5 times per power cycle: each failed attempt costs one
//     ~4 s reboot, a success keeps the radio alive for the whole session.
//  3. Keep the driver started forever: esp_wifi_stop/deinit are
//     linker-wrapped to no-ops (see config.ini --wrap flags), so the app's
//     WiFi.mode(WIFI_OFF) cycles never power the radio down again.
//  4. If the rail still can't power the modem, fail gracefully: WiFi/BLE
//     entry points refuse with an error instead of browning the board out.
//  5. Telemetry goes to both consoles (native USB + UART0): the UART bridge
//     survives radio power-up, the native USB console usually does not.
//

#pragma once

#include <esp_wifi.h>

class RadioKeeper
{
public:
  // Bring the radio up with minimal load (call early in setup(), before the
  // display initializes). Safe to call on every boot.
  static void warmUp();

  // True once the driver has been started and is being kept alive.
  static bool isUp() { return s_radioUp; }

  // True when the warm-up gave up this power cycle (rail can't power the
  // modem). WiFi calls then fail gracefully instead of browning out.
  static bool gaveUp() { return s_gaveUp; }

private:
  static bool s_radioUp;
  static bool s_gaveUp;
};
