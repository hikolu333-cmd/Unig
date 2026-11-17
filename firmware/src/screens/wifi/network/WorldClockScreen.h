//
// Created by L Shaf on 2026-03-01.
//

#pragma once

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "ui/templates/BaseScreen.h"
#include <WiFi.h>
#include <time.h>
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class WorldClockScreen : public BaseScreen
{
public:
  const char* title() override { return "World Clock"; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  static constexpr int MIN_OFFSET = -12 * 60;
  static constexpr int MAX_OFFSET =  14 * 60;
  static constexpr int STEP       =  30;

  int      _offsetMinutes  = 0;
  uint32_t _lastRenderTime = 0;
  bool     _synced         = false;
  bool     _chromeDrawn    = false;
#ifdef USE_LVGL_SHELL
  lv_obj_t* _lblOffset = nullptr;   // "UTC +h:mm" (grey, above center)
  lv_obj_t* _lblTime   = nullptr;   // "%H:%M:%S" (large, center)
  lv_obj_t* _lblFooter = nullptr;   // "[Up/Dn] Offset" control hint
#endif

  void _adjustOffset(int delta) {
    _offsetMinutes += delta;
    if (_offsetMinutes > MAX_OFFSET) _offsetMinutes = MIN_OFFSET;
    if (_offsetMinutes < MIN_OFFSET) _offsetMinutes = MAX_OFFSET;
    render();
  }

  void _back();
};