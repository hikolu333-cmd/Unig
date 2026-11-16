#include "RadioKeeper.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <stdarg.h>
#include "esp_phy_init.h"   // RF calibration mode downgrade on weak rails

bool RadioKeeper::s_gaveUp  = false;
bool RadioKeeper::s_railWeak = false;

// Bridge for main.cpp (shared code can't include board headers).
void diyJoystickRadioBootCheck() { RadioKeeper::bootCheck(); }

// Dual-channel telemetry: the native USB console (HWCDC on USB-C) can die
// when the radio powers up, so every RadioKeeper line is mirrored to UART0 —
// the CH9102 bridge on the micro-USB port, which survives.
static void rkPrintf(const char* fmt, ...) {
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
  Serial0.print(buf);
  Serial.flush();
  Serial0.flush();
}

// NVS helpers that skip the (wear-relevant) write when nothing changes.
static void rkPutBool(Preferences& p, const char* k, bool v) {
  if (p.getBool(k, !v) != v) p.putBool(k, v);
}

// ── linker-wrapped radio API (see config.ini -Wl,--wrap=...) ──────────────
// The radio is strictly on-demand: only esp_wifi_start and the PHY init are
// wrapped (weak-rail insurance); init/stop/deinit pass straight through, so
// WiFi.mode(WIFI_OFF) really powers the modem down and an idle board stays
// cool.
extern "C" {

  esp_err_t __real_esp_wifi_start(void);
  esp_err_t __wrap_esp_wifi_start(void) {
    if (RadioKeeper::gaveUp()) {
      rkPrintf("[RADIO] wifi start refused — rail gave up this power cycle\n");
      return ESP_ERR_WIFI_STATE;
    }
    // Mark the attempt so a brownout mid-start is counted by the next boot.
    {
      Preferences prefs;
      prefs.begin("radio", false);
      rkPutBool(prefs, "attempting", true);
      prefs.end();
    }
    esp_err_t e = __real_esp_wifi_start();
    {
      Preferences prefs;
      prefs.begin("radio", false);
      rkPutBool(prefs, "attempting", false);
      if (e == ESP_OK) rkPutBool(prefs, "fails", 0);  // healthy start
      prefs.end();
    }
    return e;
  }

  // The full/partial RF calibration inside PHY init sweeps the PA at high
  // current (300-500 mA) — the exact profile a weak rail can't deliver. On
  // boards where a brownout was ever counted, downgrade every requested
  // calibration to PHY_RF_CAL_NONE (the deep-sleep wake path): the radio
  // comes up uncalibrated but alive. Healthy boards get the real thing.
  int __real_register_chipv7_phy(const esp_phy_init_data_t* init_data,
                                 esp_phy_calibration_data_t* cal_data,
                                 esp_phy_calibration_mode_t cal_mode);
  int __wrap_register_chipv7_phy(const esp_phy_init_data_t* init_data,
                                 esp_phy_calibration_data_t* cal_data,
                                 esp_phy_calibration_mode_t cal_mode) {
    if (cal_mode != PHY_RF_CAL_NONE && RadioKeeper::railWeak()) {
      rkPrintf("[RADIO] weak rail: RF cal %d -> NONE (dodge the PA spike)\n", (int)cal_mode);
      cal_mode = PHY_RF_CAL_NONE;
    }
    return __real_register_chipv7_phy(init_data, cal_data, cal_mode);
  }
}

void RadioKeeper::bootCheck()
{
  esp_reset_reason_t reason = esp_reset_reason();

  // Classify the previous boot:
  //   attempting=1 + POWERON/BROWNOUT -> died mid-wifi-start -> fails++
  //   attempting=0 + POWERON/BROWNOUT -> clean power cycle   -> fails=0
  //   fails>=5 -> refuse starts until the next power cycle
  Preferences prefs;
  prefs.begin("radio", false);
  bool attempting = prefs.getBool("attempting", false);
  uint8_t fails = prefs.getUChar("fails", 0);
  if (attempting &&
      (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT)) {
    if (fails < 255) { prefs.putUChar("fails", fails + 1); fails++; }
    // One brownout mid-start is enough to mark the rail as weak (sticky).
    rkPutBool(prefs, "railweak", true);
  } else if (!attempting &&
             (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT)) {
    if (fails != 0) prefs.putUChar("fails", 0);
    fails = 0;
  }
  if (fails >= 5) {
    rkPutBool(prefs, "attempting", false);
    prefs.end();
    s_gaveUp = true;
  } else {
    prefs.end();
    s_gaveUp = false;
  }
  s_railWeak = s_gaveUp;   // refused implies weak; otherwise read the sticky flag
  {
    Preferences p2;
    p2.begin("radio", true);
    if (p2.getBool("railweak", false)) s_railWeak = true;
    p2.end();
  }

  rkPrintf("[RADIO] on-demand radio: fails=%d/5%s heap=%u psram=%u chip=%.1fC\n",
           (int)fails, s_gaveUp ? " — REFUSED this power cycle" : "",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getPsramSize(),
           temperatureRead());
}

void RadioKeeper::resetFails()
{
  Preferences prefs;
  prefs.begin("radio", false);
  rkPutBool(prefs, "attempting", false);
  if (prefs.getUChar("fails", 0) != 0) prefs.putUChar("fails", 0);
  rkPutBool(prefs, "railweak", false);
  prefs.end();
  s_gaveUp = false;
  s_railWeak = false;
  rkPrintf("[R] radio state cleared — next WiFi/BT screen retries from scratch\n");
}
