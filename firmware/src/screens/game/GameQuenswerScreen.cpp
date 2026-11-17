//
// Quenswer — native LVGL build (USE_LVGL_SHELL). Answer blanks as rounded
// cells; setup via stick char-cycler; handoff panel hides the word from P2.
//

#include "GameQuenswerScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#ifdef USE_LVGL_SHELL

void GameQuenswerScreen::onInit()
{
  _bestScore = Config.get("quenswer_best", "0").toInt();
  _build();
}

void GameQuenswerScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();

  _arena = lv_obj_create(parent);
  lv_obj_set_pos(_arena, 0, 0);
  lv_obj_set_size(_arena, lv_obj_get_width(parent), lv_obj_get_height(parent));
  lv_obj_set_style_bg_color(_arena, lv_color_hex(0x100f1a), 0);
  lv_obj_set_style_bg_opa(_arena, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_arena, 0, 0);
  lv_obj_set_style_pad_all(_arena, 0, 0);
  lv_obj_set_style_radius(_arena, 0, 0);
  lv_obj_clear_flag(_arena, LV_OBJ_FLAG_SCROLLABLE);

  _title = lv_label_create(_arena);
  lv_obj_set_style_text_font(_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(_title, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_align(_title, LV_ALIGN_TOP_MID, 0, 4);
  lv_label_set_text(_title, "QUENSWER");

  const int cellW = 30, cellH = 40, gap = 5;
  const int w = kMaxLen * cellW + (kMaxLen - 1) * gap;
  _cellsCont = lv_obj_create(_arena);
  lv_obj_align(_cellsCont, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_size(_cellsCont, w, cellH);
  lv_obj_set_style_bg_opa(_cellsCont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_cellsCont, 0, 0);
  lv_obj_set_style_pad_all(_cellsCont, 0, 0);
  lv_obj_clear_flag(_cellsCont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(_cellsCont, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(_cellsCont, gap, 0);

  for (int i = 0; i < kMaxLen; i++) {
    _cells[i] = lv_label_create(_cellsCont);
    lv_obj_set_size(_cells[i], cellW, cellH);
    lv_obj_set_style_text_font(_cells[i], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(_cells[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(_cells[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_cells[i], lv_color_hex(0x1a2030), 0);
    lv_obj_set_style_radius(_cells[i], 6, 0);
    lv_label_set_text(_cells[i], " ");
  }

  _status = lv_label_create(_arena);
  lv_obj_set_style_text_font(_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_status, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(_status, LV_ALIGN_TOP_MID, 0, 96);
  lv_label_set_text(_status, "");

  _hint = lv_label_create(_arena);
  lv_obj_set_style_text_font(_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_hint, lv_color_hex(0x5b6673), 0);
  lv_obj_align(_hint, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_label_set_text(_hint, "UP/DOWN = letter   LEFT/RIGHT = move   PRESS = place   hold BACK = del");
}

void GameQuenswerScreen::_refreshCells()
{
  for (int i = 0; i < kMaxLen; i++) {
    if (i >= _answerLen) { lv_obj_add_flag(_cells[i], LV_OBJ_FLAG_HIDDEN); continue; }
    lv_obj_clear_flag(_cells[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* c = _cells[i];
    if (_state == ST_SETUP) {
      bool cur = (i == _pos);
      char buf[2] = { cur ? _letter : ' ', 0 };
      lv_label_set_text(c, buf);
      lv_obj_set_style_bg_color(c, cur ? lv_color_hex(0x2a3654) : lv_color_hex(0x1a2030), 0);
      lv_obj_set_style_text_color(c, lv_color_white(), 0);
    } else {
      char ch = _revealed[i] ? _answer[i] : '_';
      char buf[2] = { ch, 0 };
      lv_label_set_text(c, buf);
      lv_obj_set_style_bg_color(c, _revealed[i] ? lv_color_hex(0x1e7a4b) : lv_color_hex(0x1a2030), 0);
      lv_obj_set_style_text_color(c, _revealed[i] ? lv_color_black() : lv_color_hex(0x8a94a6), 0);
    }
  }
}

void GameQuenswerScreen::_cycle(int dir)
{
  _letter = (dir > 0) ? (_letter == 'Z' ? 'A' : _letter + 1)
                     : (_letter == 'A' ? 'Z' : _letter - 1);
  _refreshCells();
}

void GameQuenswerScreen::_confirmChar()
{
  if (_pos >= kMaxLen) return;
  _answer[_pos++] = _letter;
  _answerLen = _pos;
  _answer[_answerLen] = '\0';
  _refreshCells();
}

void GameQuenswerScreen::_backspace()
{
  if (_pos > 0) {
    _pos--;
    _answerLen = _pos;
    _answer[_pos] = '\0';
    _refreshCells();
  }
}

void GameQuenswerScreen::_finishSetup()
{
  if (_answerLen < 3) {
    lv_label_set_text(_status, "answer needs at least 3 letters");
    _flashUntil = millis() + 1200;
    return;
  }
  memset(_revealed, 0, sizeof(_revealed));
  _used = 0;
  _showHandoff();
}

void GameQuenswerScreen::_showHandoff()
{
  _state = ST_HANDOFF;
  lv_label_set_text(_hint, "");
  lv_label_set_text(_status, "");
  // hide the word from player 2
  for (int i = 0; i < kMaxLen; i++) lv_label_set_text(_cells[i], " ");

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 240, 140);
  lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x101a26), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  _ovTitle = lv_label_create(_overlay);
  lv_obj_set_style_text_font(_ovTitle, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(_ovTitle, lv_color_white(), 0);
  lv_obj_align(_ovTitle, LV_ALIGN_TOP_MID, 0, 10);
  lv_label_set_text(_ovTitle, "PASS TO PLAYER 2");

  char buf[48];
  snprintf(buf, sizeof(buf), "%u letters   %u attempts", _answerLen, _budget);
  _ovSub = lv_label_create(_overlay);
  lv_obj_set_style_text_font(_ovSub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_ovSub, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(_ovSub, LV_ALIGN_TOP_MID, 0, 44);
  lv_label_set_text(_ovSub, buf);

  lv_obj_t* h = lv_label_create(_overlay);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 72);
  lv_label_set_text(h, "PRESS = start guessing");
}

void GameQuenswerScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameQuenswerScreen::_guess()
{
  char g = _letter;
  bool hit = false;
  for (int i = 0; i < _answerLen; i++) {
    if (!_revealed[i] && _answer[i] == g) { _revealed[i] = 1; hit = true; }
  }
  _used++;
  _refreshCells();

  bool solved = true;
  for (int i = 0; i < _answerLen; i++) if (!_revealed[i]) solved = false;

  char buf[64];
  if (solved) {
    int score = (int)_answerLen * 10 + (int)(_budget - _used) * 20;
    if (score > _bestScore) {
      _bestScore = score;
      Config.set("quenswer_best", String(score));
    }
    int n = Achievement.inc("quenswer_solve");
    if (n == 1) Achievement.unlock("quenswer_solve");
    _showResult(true);
    return;
  }
  snprintf(buf, sizeof(buf), "%s — attempts %u / %u",
           hit ? "hit!" : "miss", _used, _budget);
  lv_label_set_text(_status, buf);
  lv_obj_set_style_text_color(_status,
    hit ? lv_color_hex(0x62d49c) : lv_color_hex(0xff7043), 0);
  if (_used >= _budget) _showResult(false);
}

void GameQuenswerScreen::_showResult(bool solved)
{
  _state = ST_RESULT;
  _hideOverlay();
  lv_label_set_text(_hint, "");
  for (int i = 0; i < kMaxLen; i++) if (i < _answerLen) _revealed[i] = 1;
  _refreshCells();

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 240, 130);
  lv_obj_set_style_bg_color(_overlay, solved ? lv_color_hex(0x0d1a12) : lv_color_hex(0x1a0d0d), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, solved ? lv_color_hex(0x2e8b57) : lv_color_hex(0xb0540f), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  _ovTitle = lv_label_create(_overlay);
  lv_obj_set_style_text_font(_ovTitle, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(_ovTitle, solved ? lv_color_hex(0x62d49c) : lv_color_hex(0xff7043), 0);
  lv_obj_align(_ovTitle, LV_ALIGN_TOP_MID, 0, 8);
  lv_label_set_text(_ovTitle, solved ? "SOLVED!" : "OUT OF ATTEMPTS");

  char buf[48];
  snprintf(buf, sizeof(buf), "best run: %d", _bestScore);
  _ovSub = lv_label_create(_overlay);
  lv_obj_set_style_text_font(_ovSub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_ovSub, lv_color_white(), 0);
  lv_obj_align(_ovSub, LV_ALIGN_TOP_MID, 0, 42);
  lv_label_set_text(_ovSub, buf);

  lv_obj_t* h = lv_label_create(_overlay);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 70);
  lv_label_set_text(h, "PRESS = new round   BACK = exit");
}

void GameQuenswerScreen::onUpdate()
{
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();

    if (_state == ST_SETUP) {
      if      (dir == INavigation::DIR_UP)    _cycle(1);
      else if (dir == INavigation::DIR_DOWN) _cycle(-1);
      else if (dir == INavigation::DIR_RIGHT) { if (_pos < kMaxLen) _refreshCells(); }
      else if (dir == INavigation::DIR_LEFT)  { if (_pos > 0) _backspace(); }
      else if (dir == INavigation::DIR_BACK)  _backspace();
      else if (dir == INavigation::DIR_PRESS) _confirmChar();
      // finishing: pressing at the last cell
      if (_pos >= kMaxLen && dir == INavigation::DIR_PRESS) _finishSetup();
    } else if (_state == ST_HANDOFF) {
      if (dir == INavigation::DIR_PRESS) {
        _hideOverlay();
        _state = ST_PLAY;
        _letter = 'A';
        _refreshCells();
        lv_label_set_text(_hint, "cycle a letter, PRESS = guess it");
        lv_label_set_text(_status, (String("attempts 0 / ") + _budget).c_str());
      } else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    } else if (_state == ST_PLAY) {
      if      (dir == INavigation::DIR_UP)    _cycle(1);
      else if (dir == INavigation::DIR_DOWN) _cycle(-1);
      else if (dir == INavigation::DIR_PRESS) _guess();
      else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    } else if (_state == ST_RESULT) {
      if (dir == INavigation::DIR_PRESS) {
        // fresh round: wipe to setup
        _hideOverlay();
        _state = ST_SETUP;
        _pos = 0; _answerLen = 0; _letter = 'A';
        memset(_answer, 0, sizeof(_answer));
        _refreshCells();
        lv_label_set_text(_status, "");
        lv_label_set_text(_hint, "UP/DOWN = letter   PRESS = place   BACK = delete");
      } else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    }
  }
}

void GameQuenswerScreen::onRender()
{
  LvglUi::pump();
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#include "legacy_game_quenswer.inc"

#endif
