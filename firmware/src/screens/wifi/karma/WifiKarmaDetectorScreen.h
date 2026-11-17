#pragma once

#include <esp_wifi.h>
#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class WifiKarmaDetectorScreen : public BaseScreen
{
public:
  const char* title() override { return "Karma Detector"; }
  bool inhibitPowerOff() override { return _scanning; }

  ~WifiKarmaDetectorScreen();

  void onInit() override;
  void onUpdate() override;
  void onRender() override;
  void onBack();

private:
  static constexpr int          PROBE_WAIT_MS = 700;
  static constexpr int          CHANNELS      = 13;
  static constexpr unsigned long LINGER_MS    = 15000UL;

  enum State { STATE_SCANNING, STATE_DETECTED };

  State         _state      = STATE_SCANNING;
  bool          _scanning   = false;
  volatile bool _newHit     = false;
  int           _channel    = 1;

  char          _fakeSSID[20]   = {};
  char          _statusLine[48] = {};
  unsigned long _lastProbe      = 0;
  unsigned long _detectedAt     = 0;

  uint8_t _hitBssid[6] = {};
  int8_t  _hitRssi     = 0;
  uint8_t _hitChannel  = 0;

  static WifiKarmaDetectorScreen* _instance;
  static void _promiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type);

  void _startScan();
  void _stopScan();
  void _sendProbe();
  void _onHit();

#ifdef USE_LVGL_SHELL
  lv_obj_t* _lblTitle  = nullptr;  // "Monitoring..." / "Karma attack detected"
  lv_obj_t* _lblInfo   = nullptr;  // BSSID (white) when detected
  lv_obj_t* _lblStatus = nullptr;  // status line / "RSSI dBm  CH:n"
#endif
};
