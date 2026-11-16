#include "RadioKeeper.h"
#include "pins_arduino.h"   // LCD_BL / TFT_CS for display quiescing
#include <Arduino.h>
#include <Preferences.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <stdarg.h>
#include "esp_phy_init.h"   // TEMP/BOARD: RF calibration mode downgrade

bool RadioKeeper::s_radioUp = false;
bool RadioKeeper::s_gaveUp = false;

// Bridge for main.cpp (shared code can't include board headers).
void diyJoystickRadioWarmUp() { RadioKeeper::warmUp(); }

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

// ── linker-wrapped esp_wifi_* API (see config.ini -Wl,--wrap=...) ──────────
// Once the radio is warm, init/start are answered with ESP_OK and stop/deinit
// become no-ops, so the radio never powers down for the whole session. Before
// the warm-up (or when it was skipped) everything passes through untouched.
extern "C" {
  esp_err_t __real_esp_wifi_init(const wifi_init_config_t* config);
  esp_err_t __real_esp_wifi_start(void);
  esp_err_t __real_esp_wifi_stop(void);
  esp_err_t __real_esp_wifi_deinit(void);

  // The full/partial RF calibration inside PHY init is what hammers the rail:
  // it sweeps the PA at high current (300-500 mA). Downgrade every requested
  // calibration to PHY_RF_CAL_NONE — the mode a deep-sleep wake uses — so the
  // radio comes up uncalibrated but alive. TX accuracy/sensitivity is
  // slightly degraded; for this board it beats a brownout on every attempt.

  int __real_register_chipv7_phy(const esp_phy_init_data_t* init_data,
                                 esp_phy_calibration_data_t* cal_data,
                                 esp_phy_calibration_mode_t cal_mode);
  int __wrap_register_chipv7_phy(const esp_phy_init_data_t* init_data,
                                 esp_phy_calibration_data_t* cal_data,
                                 esp_phy_calibration_mode_t cal_mode) {
    if (cal_mode != PHY_RF_CAL_NONE) {
      rkPrintf("[RADIO] RF cal mode %d -> NONE (dodge the PA spike)\n", (int)cal_mode);
      cal_mode = PHY_RF_CAL_NONE;
    }
    int rc = __real_register_chipv7_phy(init_data, cal_data, cal_mode);
    rkPrintf("[RADIO] PHY init done rc=%d\n", rc);   // bisect: died here?
    return rc;
  }

  esp_err_t __wrap_esp_wifi_init(const wifi_init_config_t* config) {
    if (RadioKeeper::isUp()) return ESP_OK;
    return __real_esp_wifi_init(config);
  }
  esp_err_t __wrap_esp_wifi_start(void) {
    if (RadioKeeper::isUp()) return ESP_OK;
    // Rail already proven unable to power the modem this cycle: fail the
    // call instead of browning the board out (screens show an error).
    if (RadioKeeper::gaveUp()) return ESP_ERR_WIFI_STATE;
    return __real_esp_wifi_start();
  }
  esp_err_t __wrap_esp_wifi_stop(void) {
    if (RadioKeeper::isUp()) return ESP_OK;   // keep the driver running
    return __real_esp_wifi_stop();
  }
  esp_err_t __wrap_esp_wifi_deinit(void) {
    if (RadioKeeper::isUp()) return ESP_OK;   // keep the radio powered
    return __real_esp_wifi_deinit();
  }
}

void RadioKeeper::warmUp()
{
  esp_reset_reason_t reason = esp_reset_reason();

  // Boot-loop guard with retries. The RF-calibration current spike is right
  // at the edge of what this devkit's supply can deliver: it sometimes
  // passes, sometimes collapses the rail. So instead of one shot we allow up
  // to 5 warm-up attempts per power cycle (each failure costs one ~3s reboot):
  //   attempting=1 + POWERON/BROWNOUT -> died mid-attempt -> fails++
  //   attempting=0 + POWERON/BROWNOUT -> clean power cycle   -> fails=0
  //   fails>=5 -> give up until the next power cycle
  Preferences prefs;
  prefs.begin("radio", false);
  bool attempting = prefs.getBool("attempting", false);
  uint8_t fails = prefs.getUChar("fails", 0);
  if (attempting &&
      (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT)) {
    prefs.putUChar("fails", fails < 255 ? fails + 1 : 255);
    fails++;
  } else if (!attempting &&
             (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT)) {
    prefs.putUChar("fails", 0);
    fails = 0;
  }
  if (fails >= 5) {
    prefs.putBool("attempting", false);
    prefs.end();
    s_gaveUp = true;
    rkPrintf("[RADIO] warm-up failed %d times — radio OFF this power cycle\n", (int)fails);
    return;
  }
  s_gaveUp = false;
  prefs.putBool("attempting", true);
  prefs.end();

  rkPrintf("[RADIO] warm-up: radio start (attempt %d/5)...\n", (int)fails + 1);

  // Quiesce the display before the spike: it is NOT initialized yet at this
  // point, so its control pins float — a floating backlight gate can leave
  // the panel LED drawing current through the whole attempt. Drive the
  // backlight LOW and deassert the panel chip select.
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, LOW);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // Minimal rail load with the display not initialized yet. NOTE: 80 MHz is
  // the floor — the WiFi driver hangs below it (task watchdog: IDLE0 starved
  // by the busy wifi task, verified on a healthy N16R8 board).
  uint32_t cpu = getCpuFrequencyMhz();
  setCpuFrequencyMhz(80);

  esp_netif_init();
  esp_event_loop_create_default();
  nvs_flash_init();
  rkPrintf("[RADIO] netif/loop/nvs ok\n");

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t e = __real_esp_wifi_init(&cfg);
  rkPrintf("[RADIO] esp_wifi_init err=%d\n", (int)e);
  if (e != ESP_OK) {
    setCpuFrequencyMhz(cpu);
    prefs.begin("radio", false);
    prefs.putBool("attempting", false);
    prefs.putUChar("fails", 0);
    prefs.end();
    return;
  }
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_STA);

  rkPrintf("[RADIO] esp_wifi_start...\n");
  e = __real_esp_wifi_start();          // ← the one-time RF calibration spike
  rkPrintf("[RADIO] esp_wifi_start err=%d heap=%u\n", (int)e, (unsigned)ESP.getFreeHeap());
  if (e != ESP_OK) {
    __real_esp_wifi_deinit();
    setCpuFrequencyMhz(cpu);
    prefs.begin("radio", false);
    prefs.putBool("attempting", false);
    prefs.putUChar("fails", 0);
    prefs.end();
    return;
  }

  s_radioUp = true;
  esp_wifi_set_max_tx_power(44);        // 11 dBm: softer TX spikes forever

  // Air-visible proof of life: softAP shows up in any nearby Wi-Fi scan even
  // if the USB console dies at PHY enable (BBPLL reconfig).
  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_netif_create_default_wifi_ap();
  wifi_config_t ap = {};
  strcpy((char*)ap.ap.ssid, "UniGeek");
  ap.ap.ssid_len = 7;
  strcpy((char*)ap.ap.password, "12345678");
  ap.ap.channel = 1;
  ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap.ap.max_connection = 2;
  esp_wifi_set_config(WIFI_IF_AP, &ap);
  rkPrintf("[RADIO] softAP UniGeek up, apDone\n");
  setCpuFrequencyMhz(cpu);

  prefs.begin("radio", false);
  prefs.putBool("attempting", false);
  prefs.putUChar("fails", 0);
  prefs.end();

  rkPrintf("[RADIO] warm-up OK, radio kept alive (heap=%u)\n", (unsigned)ESP.getFreeHeap());
}
