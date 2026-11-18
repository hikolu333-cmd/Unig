//
// IR Brand Zap — native LVGL screen (USE_LVGL_SHELL). Class → brand →
// command panel (Power/Mute/Vol±) → non-blocking volley, one code per
// frame so BACK cancels and LVGL keeps pumping.
//

#include "IRBrandZapScreen.h"
#include "core/Device.h"
#include "core/PinConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#ifdef USE_LVGL_SHELL

void IRBrandZapScreen::onInit()
{
  _txPin = (int8_t)PinConfig.getInt(PIN_CONFIG_IR_TX, PIN_CONFIG_IR_TX_DEFAULT);
  _build();
  _showCats();
}

void IRBrandZapScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();
  _arena = gamekit::arena(parent);   // dark theme default

  // viewParent may still be 0-sized during onInit (layout not yet run) —
  // arena() already forces the layout, but keep the guard for the rest.
  int w = lv_obj_get_width(_arena);
  int h = lv_obj_get_height(_arena);
  if (w < 100) w = 320;
  if (h < 100) h = 206;

  _status = gamekit::label(_arena, &lv_font_montserrat_14, 0xffb380,
                            LV_ALIGN_BOTTOM_MID, 0, -2, "");

  _bar = lv_obj_create(_arena);
  lv_obj_set_pos(_bar, 20, h - 26);
  lv_obj_set_size(_bar, 0, 8);
  lv_obj_set_style_bg_color(_bar, lv_color_hex(0xffd54f), 0);
  lv_obj_set_style_bg_opa(_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_bar, 0, 0);
  lv_obj_set_style_radius(_bar, 4, 0);
  lv_obj_add_flag(_bar, LV_OBJ_FLAG_HIDDEN);

  _list = lv_obj_create(_arena);
  lv_obj_set_pos(_list, 6, 6);
  lv_obj_set_size(_list, w - 12, h - 36);
  lv_obj_set_style_bg_opa(_list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_list, 0, 0);
  lv_obj_set_style_pad_all(_list, 0, 0);
  lv_obj_set_style_pad_row(_list, 6, 0);
  lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scrollbar_mode(_list, LV_SCROLLBAR_MODE_OFF);
}

void IRBrandZapScreen::_fillList(const char* const* labels, int n, int accentIdx)
{
  lv_obj_clean(_list);
  _rowCount = 0;
  for (int i = 0; i < n && _rowCount < kMaxRows; i++) {
    uint32_t color = (i == accentIdx) ? 0xffb380 : 0xffffff;
    _rows[_rowCount++] = gamekit::label(_list, &lv_font_montserrat_16, color,
                                        LV_ALIGN_TOP_LEFT, 0, 0, labels[i]);
  }
  lv_obj_add_flag(_bar, LV_OBJ_FLAG_HIDDEN);
}

void IRBrandZapScreen::_focusRow(int i)
{
  for (int r = 0; r < _rowCount; r++)
    gamekit::focusRow(_rows[r], r == i);
  if (i >= 0 && i < _rowCount)
    lv_obj_scroll_to_view(_rows[i], LV_ANIM_OFF);
}

void IRBrandZapScreen::_showCats()
{
  _state = ST_CATS;
  _sel = 0;

  int tv = BrandZap::brandCount(BrandZap::CAT_TV);
  int pj = BrandZap::brandCount(BrandZap::CAT_PROJ);
  char bTv[40], bPj[40], bAll[48];
  snprintf(bTv, sizeof(bTv), "TV Brands  (%d)", tv);
  snprintf(bPj, sizeof(bPj), "Projector Brands  (%d)", pj);
  snprintf(bAll, sizeof(bAll), "ALL PROJECTORS  (%d codes)",
           BrandZap::zapAllProjectorsTotal());
#ifndef BOARD_4MB
  char bU[48];
  int nP = UniversalTv::btnTotal(UniversalTv::BTN_POWER);
  snprintf(bU, sizeof(bU), "UNIVERSAL TV  (%d)", nP);
  const char* labels[kCatRows] = { bU, bTv, bPj, bAll };
  _fillList(labels, kCatRows, 0);
#else
  const char* labels[kCatRows] = { bTv, bPj, bAll };
  _fillList(labels, kCatRows, 2);
#endif
  lv_label_set_text(_status, "pick a target class");
  _focusRow(0);
}

#ifndef BOARD_4MB
void IRBrandZapScreen::_showUButtons()
{
  _state = ST_UBTN;
  _sel = 0;

  static char bufs[UniversalTv::BTN_N][40];
  const char* labels[UniversalTv::BTN_N];
  for (int i = 0; i < UniversalTv::BTN_N; i++) {
    snprintf(bufs[i], sizeof(bufs[i]), "%s  (%d)",
             UniversalTv::BTN_NAMES[i], UniversalTv::btnTotal(i));
    labels[i] = bufs[i];
  }
  _fillList(labels, UniversalTv::BTN_N, -1);
  lv_label_set_text(_status, "flipper-style universal TV remote");
  _focusRow(0);
}
#endif

void IRBrandZapScreen::_showBrands()
{
  _state = ST_BRANDS;
  _sel = 0;

  static char bufs[kMaxRows][44];
  const char* labels[kMaxRows];
  int n = 0;
  for (int i = 0; i < BrandZap::ENTRY_COUNT && n < kMaxRows; i++) {
    const BrandZap::Entry* e = &BrandZap::ENTRIES[i];
    if (e->cat != _cat) continue;
    int np = e->ranges[BrandZap::CMD_POWER].count;
    int extra = e->ranges[BrandZap::CMD_MUTE].count
              + e->ranges[BrandZap::CMD_VOLUP].count
              + e->ranges[BrandZap::CMD_VOLDN].count;
    snprintf(bufs[n], sizeof(bufs[n]), "%s  (%d%s)", e->brand, np,
             extra ? "+av" : "");
    labels[n] = bufs[n];
    n++;
  }
  _fillList(labels, n, -1);
  char sb[48];
  snprintf(sb, sizeof(sb), "%s: pick the brand",
           _cat == BrandZap::CAT_TV ? "TVs" : "Projectors");
  lv_label_set_text(_status, sb);
  _focusRow(0);
}

void IRBrandZapScreen::_showCmds()
{
  _state = ST_CMDS;
  _sel = 0;

  int np  = _entry->ranges[BrandZap::CMD_POWER].count;
  int nm  = _entry->ranges[BrandZap::CMD_MUTE].count;
  int nvu = _entry->ranges[BrandZap::CMD_VOLUP].count;
  int nvd = _entry->ranges[BrandZap::CMD_VOLDN].count;

  static char bufs[kCmdRows][44];
  snprintf(bufs[0], sizeof(bufs[0]), "POWER OFF  (%d)", np);
  snprintf(bufs[1], sizeof(bufs[1]), "POWER ON  (%d)", np);
  snprintf(bufs[2], sizeof(bufs[2]), "MUTE  (%d)", nm);
  snprintf(bufs[3], sizeof(bufs[3]), "VOL +  (%d)", nvu);
  snprintf(bufs[4], sizeof(bufs[4]), "VOL -  (%d)", nvd);
  // hide commands the library doesn't have for this brand
  const char* labels[kCmdRows];
  int n = 0;
  int map[kCmdRows];
  if (np)  { labels[n] = bufs[0]; map[n] = BrandZap::CMD_POWER; n++; }
  if (np)  { labels[n] = bufs[1]; map[n] = BrandZap::CMD_POWER; n++; }
  if (nm)  { labels[n] = bufs[2]; map[n] = BrandZap::CMD_MUTE;  n++; }
  if (nvu) { labels[n] = bufs[3]; map[n] = BrandZap::CMD_VOLUP; n++; }
  if (nvd) { labels[n] = bufs[4]; map[n] = BrandZap::CMD_VOLDN; n++; }
  memcpy(_map, map, sizeof(map));
  _cmdCount = n;

  _fillList(labels, n, -1);
  char sb[56];
  snprintf(sb, sizeof(sb), "%s: pick the command", _entry->brand);
  lv_label_set_text(_status, sb);
  _focusRow(0);
}

void IRBrandZapScreen::_startZap(int cmdId)
{
  _ir.beginTx(_txPin);
  _cmdId = cmdId;
  _zapEntry = (int)(_entry - BrandZap::ENTRIES);
  _zapPos = 0;
  _zapTotal = _entry->ranges[cmdId].count;
  _state = ST_ZAP;
  _zapUi();
}

void IRBrandZapScreen::_startAllProjectors()
{
  _ir.beginTx(_txPin);
  _cmdId = BrandZap::CMD_POWER;
  _zapEntry = -1;
  _zapPos = 0;
  _zapTotal = BrandZap::zapAllProjectorsTotal();
  _state = ST_ZAP;
  _zapUi();
}

#ifndef BOARD_4MB
void IRBrandZapScreen::_startUniversal(int btn)
{
  _ir.beginTx(_txPin);
  _uBtn = btn;
  _zapEntry = -1;
  _zapPos = 0;
  _zapTotal = UniversalTv::btnTotal(btn);
  _state = ST_ZAP;
  _zapUi();
}

void IRBrandZapScreen::_fireUniversalStep()
{
  const UniversalTv::Btn& b = UniversalTv::BTNS[_uBtn];
  int codesN = b.codesN;

  if (_zapPos >= codesN + b.rawsN) { _finishZap("done"); return; }

  char buf[64];
  if (_zapPos < codesN) {
    const UniversalTv::UCode& c = b.codes[_zapPos];
    Serial.printf("[UTV] %s %s a=%06lX c=%06lX\n", UniversalTv::BTN_NAMES[_uBtn],
                  c.proto, (unsigned long)c.address, (unsigned long)c.command);
    // Instant-Off pacing: single frame for NEC-family, double for bi-phase
    bool biphase = !strcmp(c.proto, "RC5") || !strcmp(c.proto, "RC6");
    IrEmit::fireOnce(*_ir.sender(), c.proto, c.address, c.command, c.bits);
    if (biphase) {
      delay(15);
      IrEmit::fireOnce(*_ir.sender(), c.proto, c.address, c.command, c.bits);
    }
    delay(15);
    snprintf(buf, sizeof(buf), "UNIVERSAL %s...  %d / %d",
             UniversalTv::BTN_NAMES[_uBtn], _zapPos + 1, _zapTotal);
  } else {
    const UniversalTv::URaw& r = b.raws[_zapPos - codesN];
    Serial.printf("[UTV] %s RAW len=%u\n", UniversalTv::BTN_NAMES[_uBtn], r.len);
    _ir.sender()->sendRaw(r.samples, r.len, 38);
    delay(40);
    snprintf(buf, sizeof(buf), "UNIVERSAL %s...  %d / %d",
             UniversalTv::BTN_NAMES[_uBtn], _zapPos + 1, _zapTotal);
  }
  lv_label_set_text(_status, buf);
  lv_obj_set_width(_bar, _zapTotal > 0 ? 280 * _zapPos / _zapTotal : 0);
  _zapPos++;
  if (_zapPos >= _zapTotal) _finishZap("done");
}
#endif

void IRBrandZapScreen::_zapUi()
{
  // current target name for the status line
  const BrandZap::Entry* e = nullptr;
  if (_zapEntry >= 0) {
    e = &BrandZap::ENTRIES[_zapEntry];
  } else {
    int pos = 0;
    for (int i = 0; i < BrandZap::ENTRY_COUNT && !e; i++) {
      const BrandZap::Entry* c = &BrandZap::ENTRIES[i];
      if (c->cat != BrandZap::CAT_PROJ) continue;
      if (pos + c->ranges[_cmdId].count > _zapPos) e = c;
      else pos += c->ranges[_cmdId].count;
    }
  }
  static const char* kCmdNames[BrandZap::CMD_N] =
    { "POWER", "MUTE", "VOL+", "VOL-" };
  char buf[64];
  snprintf(buf, sizeof(buf), "%s %s...  %d / %d",
           e ? e->brand : "?", kCmdNames[_cmdId], _zapPos, _zapTotal);
  lv_label_set_text(_status, buf);

  lv_obj_clear_flag(_bar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_width(_bar, _zapTotal > 0 ? 280 * _zapPos / _zapTotal : 0);
}

void IRBrandZapScreen::_fireStep()
{
  // resolve (entry, codeIdx) from the linear cursor
  const BrandZap::Entry* e = nullptr;
  int codeIdx = 0;
  if (_zapEntry >= 0) {
    e = &BrandZap::ENTRIES[_zapEntry];
    codeIdx = _zapPos;
  } else {
    int pos = 0;
    for (int i = 0; i < BrandZap::ENTRY_COUNT; i++) {
      const BrandZap::Entry* c = &BrandZap::ENTRIES[i];
      if (c->cat != BrandZap::CAT_PROJ) continue;
      const BrandZap::Range& r = c->ranges[_cmdId];
      if (pos + r.count > _zapPos) { e = c; codeIdx = _zapPos - pos; break; }
      pos += r.count;
    }
  }
  if (!e) { _finishZap("done"); return; }
  const BrandZap::Range& rg = e->ranges[_cmdId];
  if (codeIdx >= rg.count) { _finishZap("done"); return; }

  const BrandZap::Code& c = BrandZap::CODES[rg.first + codeIdx];
  Serial.printf("[BZ] %s %s a=%06lX c=%06lX b=%u\n",
                e->brand, c.proto,
                (unsigned long)c.address, (unsigned long)c.command, c.bits);
  if (!strcmp(c.proto, "RAW")) {
    // raw-only brand (Xiaomi): address indexes the RAW_SIGS table
    if (c.address < (uint32_t)BrandZap::RAW_SIG_COUNT) {
      BrandZap::fireRaw(*_ir.sender(), BrandZap::RAW_SIGS[c.address]);
      delay(40);
    }
  } else {
    // single frame + 15 ms gap — same pacing as Instant Off (the Vestel/
    // DEXP decoder rejects back-to-back double frames as noise). Bi-phase
    // RC5/RC6 still double up: their decoders need two frames to lock.
    bool biphase = !strcmp(c.proto, "RC5") || !strcmp(c.proto, "RC6");
    IrEmit::fireOnce(*_ir.sender(), c.proto, c.address, c.command, c.bits);
    if (biphase) {
      delay(15);
      IrEmit::fireOnce(*_ir.sender(), c.proto, c.address, c.command, c.bits);
    }
    delay(15);
  }
  _zapPos++;
  if (_zapPos >= _zapTotal) _finishZap("done");
  else _zapUi();
}

void IRBrandZapScreen::_finishZap(const char* msg)
{
  _ir.end();
  char buf[56];
  snprintf(buf, sizeof(buf), "%s: %d codes", msg, _zapPos);
  lv_obj_add_flag(_bar, LV_OBJ_FLAG_HIDDEN);
#ifndef BOARD_4MB
  if (_uBtn >= 0) { _uBtn = -1; _showUButtons(); }
  else
#endif
  if (_allProj) _showCats();
  else if (_entry) _showCmds();
  else _showCats();
  lv_label_set_text(_status, buf);

  int n = Achievement.inc("ir_brand_zap");
  if (n == 1) Achievement.unlock("ir_brand_zap");
}

void IRBrandZapScreen::onUpdate()
{
  if (_state == ST_ZAP) {
    auto d = _nav.poll();
    if (d == INavigation::DIR_BACK) {
      _ir.end();
#ifndef BOARD_4MB
      if (_uBtn >= 0) { _uBtn = -1; _showUButtons(); }
      else
#endif
      if (_allProj) _showCats();
      else _showCmds();
      lv_label_set_text(_status, "cancelled");
      return;
    }
#ifndef BOARD_4MB
    if (_uBtn >= 0) _fireUniversalStep();
    else
#endif
    _fireStep();
    return;
  }

  auto dir = _nav.poll();
  if (dir == INavigation::DIR_NONE) return;

  if (_state == ST_CATS) {
    if      (dir == INavigation::DIR_UP)   _sel = (_sel + kCatRows - 1) % kCatRows;
    else if (dir == INavigation::DIR_DOWN) _sel = (_sel + 1) % kCatRows;
    else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    else if (dir == INavigation::DIR_PRESS) {
#ifndef BOARD_4MB
      if (_sel == 0) {
        if (_txPin < 0) { lv_label_set_text(_status, "TX pin not set"); return; }
        _showUButtons();
      } else if (_sel == 1) { _cat = BrandZap::CAT_TV;   _allProj = false; _showBrands(); }
      else if (_sel == 2) { _cat = BrandZap::CAT_PROJ; _allProj = false; _showBrands(); }
      else {
        if (_txPin < 0) { lv_label_set_text(_status, "TX pin not set"); return; }
        _allProj = true;
        _entry = nullptr;
        _startAllProjectors();
      }
#else
      if      (_sel == 0) { _cat = BrandZap::CAT_TV;   _allProj = false; _showBrands(); }
      else if (_sel == 1) { _cat = BrandZap::CAT_PROJ; _allProj = false; _showBrands(); }
      else {
        if (_txPin < 0) { lv_label_set_text(_status, "TX pin not set"); return; }
        _allProj = true;
        _entry = nullptr;
        _startAllProjectors();
      }
#endif
    }
    _focusRow(_sel);
  } else if (_state == ST_UBTN) {
#ifndef BOARD_4MB
    if      (dir == INavigation::DIR_UP)   _sel = (_sel + UniversalTv::BTN_N - 1) % UniversalTv::BTN_N;
    else if (dir == INavigation::DIR_DOWN) _sel = (_sel + 1) % UniversalTv::BTN_N;
    else if (dir == INavigation::DIR_BACK) { _showCats(); return; }
    else if (dir == INavigation::DIR_PRESS) {
      if (_txPin < 0) { lv_label_set_text(_status, "TX pin not set"); return; }
      _entry = nullptr;
      _allProj = false;
      _startUniversal(_sel);
    }
    _focusRow(_sel);
#endif
  } else if (_state == ST_BRANDS) {
    if      (dir == INavigation::DIR_UP)   _sel = (_sel + _rowCount - 1) % _rowCount;
    else if (dir == INavigation::DIR_DOWN) _sel = (_sel + 1) % _rowCount;
    else if (dir == INavigation::DIR_BACK) { _showCats(); return; }
    else if (dir == INavigation::DIR_PRESS) {
      // map the row index back to an ENTRIES index of the category
      int k = 0;
      for (int i = 0; i < BrandZap::ENTRY_COUNT; i++) {
        const BrandZap::Entry* e = &BrandZap::ENTRIES[i];
        if (e->cat != _cat) continue;
        if (k == _sel) { _entry = e; _allProj = false; _showCmds(); return; }
        k++;
      }
    }
    _focusRow(_sel);
  } else if (_state == ST_CMDS) {
    if      (dir == INavigation::DIR_UP)   _sel = (_sel + _cmdCount - 1) % _cmdCount;
    else if (dir == INavigation::DIR_DOWN) _sel = (_sel + 1) % _cmdCount;
    else if (dir == INavigation::DIR_BACK) { _showBrands(); return; }
    else if (dir == INavigation::DIR_PRESS) {
      if (_txPin < 0) { lv_label_set_text(_status, "TX pin not set"); return; }
      _startZap(_map[_sel]);   // panel row → CMD_POWER / CMD_MUTE / CMD_VOL±
    }
    _focusRow(_sel);
  }
}

void IRBrandZapScreen::onRender()
{
  // Live updates happen in onUpdate().
}
#endif  // USE_LVGL_SHELL
