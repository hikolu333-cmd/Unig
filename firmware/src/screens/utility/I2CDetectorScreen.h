#pragma once
#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class I2CDetectorScreen : public BaseScreen {
public:
  const char* title() override { return "I2C Detector"; }
  void onInit() override;
  void onUpdate() override;
  void onRender() override;

private:
  uint8_t _wireIndex = 0;
  bool    _hasBoth = false;  // true when both ExI2C and InI2C exist
  bool    _scanning = false;
  bool    _found[0x78] = {};
  void _scan();

#ifdef USE_LVGL_SHELL
  // Address grid as an LVGL widget matrix (16×8 cells, 0x08..0x77 visible).
  lv_obj_t* _grid     = nullptr;
  lv_obj_t* _lblBus   = nullptr;   // "External (SDA:x SCL:y)" / "Internal"
  lv_obj_t* _lblScan  = nullptr;   // "Scanning..." (visible while scanning)
  lv_obj_t* _lblFooter = nullptr;  // "[Up/Dn] Bus" when bus switching exists
  lv_obj_t* _cells[128]   = {};
  lv_obj_t* _cellLbl[128] = {};
  void _buildLv();
#endif
};
