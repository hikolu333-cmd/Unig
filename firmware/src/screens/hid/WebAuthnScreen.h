#pragma once

#include <Arduino.h>  // pulls pins_arduino.h so DEVICE_HAS_WEBAUTHN is defined

#ifdef DEVICE_HAS_WEBAUTHN

#include "ui/templates/BaseScreen.h"
#include "utils/webauthn/USBFidoUtil.h"
#include "utils/webauthn/Ctaphid.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class WebAuthnScreen : public BaseScreen {
public:
  const char* title()    override { return "WebAuthn"; }
  bool inhibitPowerOff() override { return true; }

  void onInit()    override;
  void onUpdate()  override;
  void onRender()  override;

private:
  enum State : uint8_t {
    ST_ACTIVE,        // idle, waiting for host transactions
    ST_PROMPTING,     // user-presence prompt rendered, waiting for press
  };

  // User-presence callback — fired synchronously from inside Ctap2 dispatch.
  // Loops polling Uni.Nav and emitting keepalives until press / timeout.
  static bool _onUserPresence(const char* rpId, void* user);

  webauthn::Ctaphid     _ctaphid;

  State        _state            = ST_ACTIVE;
  bool         _profileMismatch  = false;   // USB taken by kbd/mouse this boot
  bool         _noMaster         = false;   // master.bin missing — needs Generate BIP39 first
  bool         _chromeDrawn      = false;
  bool         _lastConnected    = false;   // tracks first paint
  uint32_t     _txCount          = 0;       // dispatched transactions
  uint32_t     _lastTxDrawn      = 0;       // tx counter value at last paint
  const char*  _promptRpId       = nullptr;
  uint32_t     _promptStartMs    = 0;

  void _onReport(const uint8_t* buf64);
  static void _onReportThunk(const uint8_t* buf64, void* user);

#ifdef USE_LVGL_SHELL
  // Centered text lines reused by every state screen (mismatch / setup /
  // prompt card); status + tx labels for the idle body.
  lv_obj_t* _lbl[5]   = {};
  lv_obj_t* _lblStat  = nullptr;  // "Connected" / "Waiting..." banner
  lv_obj_t* _lblTx    = nullptr;  // "Tx: N" counter line
  lv_obj_t* _lvLine(int slot, const char* text, uint16_t color, int y, bool big);
  void _lvHideLines();
#endif
};

#endif  // DEVICE_HAS_WEBAUTHN
