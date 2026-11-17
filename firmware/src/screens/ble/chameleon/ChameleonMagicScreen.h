#pragma once
#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class ChameleonMagicScreen : public BaseScreen {
public:
  const char* title() override { return "Detect Magic"; }
  bool inhibitPowerOff() override { return _running; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum Phase : uint8_t { WAITING, SCANNING, RESULT };
  Phase  _phase     = WAITING;
  bool   _running   = false;
  bool   _started   = false;
  bool   _needsDraw = true;
  String _result;

#ifdef USE_LVGL_SHELL
  lv_obj_t* _lblResult = nullptr;  // detection result (white, centered)
#endif

  void _run();
};
