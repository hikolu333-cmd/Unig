#pragma once

#include "ui/templates/BaseScreen.h"

#ifdef USE_LVGL_SHELL
// ─────────────────────────────────────────────────────────────────────────────
// IR Brand Zap — brand-targeted volley (IRDB library + Vestel/DEXP zone).
// Class → brand → command panel (POWER OFF / POWER ON / MUTE / VOL+ / VOL-)
// → non-blocking volley: one code per frame, BACK cancels, live progress.
// "ALL PROJECTORS" sweeps every projector brand's POWER set in one go.
// ─────────────────────────────────────────────────────────────────────────────
#include "utils/ir/BrandZap.h"
#include "utils/ir/UniversalTv.h"
#include "utils/ir/IRUtil.h"
#include "ui/lvgl/GameNav.h"

class IRBrandZapScreen : public BaseScreen
{
public:
  const char* title()     override { return "Brand Zap"; }
  bool inhibitPowerSave() override { return true; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_CATS, ST_UBTN, ST_BRANDS, ST_CMDS, ST_ZAP };

  #ifdef BOARD_4MB
  static constexpr int kCatRows = 3;   // TVs / Projectors / ALL PROJECTORS
#else
  static constexpr int kCatRows = 4;   // + UNIVERSAL TV (Flipper asset)
#endif
  static constexpr int kCmdRows     = 5;   // OFF / ON / MUTE / VOL+ / VOL-
  static constexpr int kMaxRows    = 128;  // full TV catalog fits (109 brands)

  State    _state = ST_CATS;
  int      _cat = 0;          // BrandZap::CAT_TV / CAT_PROJ
  bool     _allProj = false;
  const BrandZap::Entry* _entry = nullptr;   // selected brand
  int      _sel = 0;
  int      _rowFirst = 0;    // ENTRIES walk helper for all-projectors

  IRUtil   _ir;
  int8_t   _txPin = -1;

  // volley cursor over the flattened code range
  int      _cmdId = BrandZap::CMD_POWER;
  int      _zapEntry = 0;    // single-brand zap: ENTRIES index (-1 = all-proj)
  int      _uBtn = -1;      // universal zap: UniversalTv button index
  int      _zapPos   = 0;    // code index within the zap's flat list
  int      _zapTotal = 0;

  GameNav  _nav;

  lv_obj_t* _arena = nullptr;
  lv_obj_t* _list  = nullptr;    // scrolling column of rows
  lv_obj_t* _status = nullptr;
  lv_obj_t* _bar = nullptr;
  lv_obj_t* _rows[kMaxRows] = {};
  int      _rowCount = 0;
  int      _rowSel = 0;          // row idx ↔ ENTRIES idx map for the category
  int      _map[kCmdRows] = {};  // visible cmd row → cmd id (0/1 off,2 mute,3 vol+,4 vol-)
  int      _cmdCount = 0;        // visible command rows

  void _build();
  void _fillList(const char* const* labels, int n, int accentIdx);
  void _showCats();
  void _showBrands();
  void _showUButtons();
  void _startUniversal(int btn);
  void _fireUniversalStep();
  void _showCmds();
  void _startZap(int cmdId);
  void _startAllProjectors();
  void _fireStep();
  void _finishZap(const char* msg);
  void _focusRow(int i);
  void _zapUi();
};
#endif
