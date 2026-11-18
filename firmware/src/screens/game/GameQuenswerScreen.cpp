//
// Quenswer — native LVGL build (USE_LVGL_SHELL). Two-player hidden-answer
// quiz: setup with an explicit FINISH button, budget picker, handoff panel
// that hides the word, hangman-style reveal for player 2.
//

#include "GameQuenswerScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)

void GameQuenswerScreen::onInit()
{
  _bestScore = Config.get("quenswer_best", "0").toInt();
  _build();
}

void GameQuenswerScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();
  _arena = gamekit::arena(parent);

  gamekit::label(_arena, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 4, "QUENSWER");

  constexpr int cellW = 28, cellH = 38, gap = 3;
  constexpr int gridW = kMaxLen * cellW + (kMaxLen - 1) * gap;   // 307
  constexpr int x0 = (320 - gridW) / 2;
  constexpr int y0 = 34;
  for (int i = 0; i < kMaxLen; i++) {
    _cells[i] = gamekit::cell(_arena, x0 + i * (cellW + gap), y0,
                              cellW, cellH, 0x2c3654, 6);
    lv_obj_add_flag(_cells[i], LV_OBJ_FLAG_HIDDEN);
  }

  // FINISH button below the row (part of the setup cursor flow)
  _finishBtn = gamekit::cell(_arena, 110, 84, 100, 30, 0x43a047, 8);
  lv_obj_set_style_text_font(_finishBtn, &lv_font_montserrat_14, 0);
  lv_label_set_text(_finishBtn, "FINISH");

  _status = gamekit::label(_arena, &lv_font_montserrat_14, 0xffb380,
                           LV_ALIGN_TOP_MID, 0, 124, "");
  _hint = gamekit::label(_arena, &lv_font_montserrat_14, 0xcfd8e3,
                         LV_ALIGN_BOTTOM_MID, 0, -2,
                         "UP/DOWN letter  PRESS place  RIGHT skip  BACK del");

  _resetSetup();
}

void GameQuenswerScreen::_resetSetup()
{
  _hideOverlay();
  _state = ST_SETUP;
  _answerLen = 0;
  _pos = 0;
  _used = 0;
  memset(_answer, 0, sizeof(_answer));
  memset(_revealed, 0, sizeof(_revealed));
  memset(_entry, 'A', sizeof(_entry));
  lv_label_set_text(_status, "player 1: type the answer");
  lv_label_set_text(_hint, "UP/DOWN letter  PRESS place  RIGHT skip  BACK del");
  _refreshCells();
}

void GameQuenswerScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameQuenswerScreen::_refreshCells()
{
  for (int i = 0; i < kMaxLen; i++) {
    lv_obj_t* cl = _cells[i];
    bool used = (i < _answerLen);

    if (_state == ST_SETUP) {
      // placed letters (green), the active cursor cell, one free cell
      // showing the cycled letter, the rest hidden.
      bool cursor = (i == _pos && _pos < kMaxLen);
      bool isFree = (i == _answerLen && _answerLen < kMaxLen);
      if (!used && !cursor && !isFree) {
        lv_obj_add_flag(cl, LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      lv_obj_clear_flag(cl, LV_OBJ_FLAG_HIDDEN);
      if (cursor && used) {
        // editing a placed cell: show the cycled replacement at the cursor
        gamekit::setCellText(cl, _entry[i]);
        gamekit::paintCell(cl, 0xffd54f, 0x0d1321, 0xffd54f);
      } else if (used) {
        gamekit::setCellText(cl, _answer[i]);
        gamekit::paintCell(cl, 0x43a047, 0x0d1321);
      } else if (cursor || isFree) {
        gamekit::setCellText(cl, _entry[i]);
        gamekit::paintCell(cl, cursor ? 0x3e5080 : 0x24345c, 0xffffff,
                          cursor ? 0xffd54f : 0x0d1321);
      }
      lv_obj_set_style_border_width(cl, cursor ? 2 : 1, 0);
    } else if (_state == ST_PLAY) {
      // exactly the answer's cells visible: blanks or revealed hits
      if (!used) { lv_obj_add_flag(cl, LV_OBJ_FLAG_HIDDEN); continue; }
      lv_obj_clear_flag(cl, LV_OBJ_FLAG_HIDDEN);
      gamekit::setCellText(cl, _revealed[i] ? _answer[i] : '_');
      if (_revealed[i]) gamekit::paintCell(cl, 0x43a047, 0x0d1321);
      else              gamekit::paintCell(cl, 0x3e5080, 0xffffff);
    } else if (_state == ST_RESULT) {
      if (!used) { lv_obj_add_flag(cl, LV_OBJ_FLAG_HIDDEN); continue; }
      lv_obj_clear_flag(cl, LV_OBJ_FLAG_HIDDEN);
      gamekit::setCellText(cl, _answer[i]);
      gamekit::paintCell(cl, _revealed[i] ? 0x43a047 : 0x55617a,
                         _revealed[i] ? 0x0d1321 : 0xffffff);
    } else {  // budget/handoff: hide the word from player 2
      if (!used) lv_obj_add_flag(cl, LV_OBJ_FLAG_HIDDEN);
      else {
        lv_obj_clear_flag(cl, LV_OBJ_FLAG_HIDDEN);
        gamekit::setCellText(cl, ' ');
        gamekit::paintCell(cl, 0x2c3654);
      }
    }
  }

  // FINISH button state (setup only)
  if (_state == ST_SETUP) {
    bool onFinish = (_pos == kMaxLen);
    bool ready = (_answerLen >= 3);
    uint32_t bg = onFinish ? (ready ? 0xffd54f : 0x55617a) : 0x43a047;
    uint32_t fg = (onFinish && ready) ? 0x0d1321 : 0xffffff;
    gamekit::paintCell(_finishBtn, bg, fg, 0x0d1321);
    lv_obj_set_style_border_width(_finishBtn, onFinish ? 3 : 1, 0);
  } else {
    gamekit::paintCell(_finishBtn, 0x24345c, 0x8fa0b8);
  }
}

void GameQuenswerScreen::_showBudget()
{
  _state = ST_BUDGET;
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 250, 140);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 12, "ATTEMPTS FOR PLAYER 2");
  char buf[24];
  snprintf(buf, sizeof(buf), "%u", _budget);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 52, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 92, "LEFT/RIGHT set   PRESS = ready");
  _refreshCells();   // hide the word
}

void GameQuenswerScreen::_showHandoff()
{
  _state = ST_HANDOFF;
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 260, 150);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 12, "PASS TO PLAYER 2");
  char buf[48];
  snprintf(buf, sizeof(buf), "%u letters   %u attempts", _answerLen, _budget);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 52, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 100, "PRESS = start guessing");
  _refreshCells();   // hide the word
}

void GameQuenswerScreen::_guess()
{
  char g = _entry[_pos];
  bool hit = false;
  for (int i = 0; i < _answerLen; i++) {
    if (!_revealed[i] && _answer[i] == g) { _revealed[i] = 1; hit = true; }
  }
  _used++;
  _refreshCells();

  bool solved = true;
  for (int i = 0; i < _answerLen; i++) if (!_revealed[i]) solved = false;

  if (solved) {
    int score = (int)_answerLen * 10 + (int)(_budget - _used) * 20;
    if (score > _bestScore) {
      _bestScore = score;
      Config.set("quenswer_best", String(score));
    }
    int n = Achievement.inc("quenswer_solve");
    if (n == 1) Achievement.unlock("quenswer_solve");
    if (Uni.Speaker) Uni.Speaker->playWin();
    _showResult(true);
    return;
  }

  char buf[64];
  snprintf(buf, sizeof(buf), "%s   attempts %u/%u",
           hit ? "HIT!" : "miss", _used, _budget);
  lv_label_set_text(_status, buf);
  lv_obj_set_style_text_color(_status,
    lv_color_hex(hit ? 0x62d49c : 0xff7043), 0);

  if (_used >= _budget) {
    if (Uni.Speaker) Uni.Speaker->playLose();
    _showResult(false);
  }
}

void GameQuenswerScreen::_showResult(bool solved)
{
  _state = ST_RESULT;
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 250, 150, solved ? 0x24422e : 0x3a2a35);
  lv_obj_set_style_border_color(_overlay,
    lv_color_hex(solved ? 0x43a047 : 0xff7043), 0);
  gamekit::label(_overlay, &lv_font_montserrat_20,
                 solved ? 0x62d49c : 0xff7043, LV_ALIGN_TOP_MID, 0, 12,
                 solved ? "SOLVED!" : "OUT OF ATTEMPTS");
  char buf[40];
  snprintf(buf, sizeof(buf), "best run: %d", _bestScore);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 52, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 92, "PRESS = new round   BACK = exit");
  _refreshCells();
}

void GameQuenswerScreen::onUpdate()
{
  auto dir = _nav.poll();
  if (dir == INavigation::DIR_NONE) return;

  if (_state == ST_SETUP) {
    if (_pos < kMaxLen) {
      if      (dir == INavigation::DIR_UP)    _entry[_pos] = (_entry[_pos] == 'Z') ? 'A' : _entry[_pos] + 1;
      else if (dir == INavigation::DIR_DOWN)  _entry[_pos] = (_entry[_pos] == 'A') ? 'Z' : _entry[_pos] - 1;
      else if (dir == INavigation::DIR_RIGHT) { if (_pos < _answerLen) _pos++; }
      else if (dir == INavigation::DIR_LEFT)  { if (_pos > 0) _pos--; }
      else if (dir == INavigation::DIR_PRESS) {
        _answer[_pos] = _entry[_pos];
        if (_pos + 1 > _answerLen) _answerLen = _pos + 1;
        _answer[_answerLen] = '\0';
        if (_pos < kMaxLen - 1) _pos++;
      }
      else if (dir == INavigation::DIR_BACK)  { if (_pos > 0) _pos--; }
      // at the last cell, RIGHT moves onto FINISH
      if (_pos == kMaxLen - 1 && dir == INavigation::DIR_RIGHT) _pos = kMaxLen;
    } else {
      // cursor on FINISH
      if      (dir == INavigation::DIR_LEFT)  _pos = kMaxLen - 1;
      else if (dir == INavigation::DIR_BACK)  _pos = kMaxLen - 1;
      else if (dir == INavigation::DIR_PRESS) {
        if (_answerLen >= 3) _showBudget();
        else {
          lv_label_set_text(_status, "answer needs at least 3 letters");
          _flashUntil = millis() + 1200;
        }
      }
    }
    if (dir == INavigation::DIR_BACK && _pos == 0 && _answerLen > 0) {
      // delete last placed letter
      _answerLen--;
      _answer[_answerLen] = '\0';
      _pos = _answerLen;
    }
    _refreshCells();
    if (_flashUntil && millis() > _flashUntil) {
      _flashUntil = 0;
      lv_label_set_text(_status, "player 1: type the answer");
      lv_obj_set_style_text_color(_status, lv_color_hex(0xffb380), 0);
    }
  } else if (_state == ST_BUDGET) {
    if      (dir == INavigation::DIR_RIGHT) _budget = min(9, _budget + 1);
    else if (dir == INavigation::DIR_UP)    _budget = min(9, _budget + 1);
    else if (dir == INavigation::DIR_LEFT)  _budget = max(3, _budget - 1);
    else if (dir == INavigation::DIR_DOWN)  _budget = max(3, _budget - 1);
    else if (dir == INavigation::DIR_BACK)  { _resetSetup(); return; }
    else if (dir == INavigation::DIR_PRESS) { _showHandoff(); return; }
    if (_overlay) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%u", _budget);
      // single label child at y=52 — update it
      lv_obj_t* val = lv_obj_get_child(_overlay, 1);
      if (val) lv_label_set_text(val, buf);
    }
  } else if (_state == ST_HANDOFF) {
    if      (dir == INavigation::DIR_PRESS) {
      _hideOverlay();
      _state = ST_PLAY;
      _pos = 0;
      memset(_entry, 'A', sizeof(_entry));
      _used = 0;
      memset(_revealed, 0, sizeof(_revealed));
      lv_label_set_text(_status, "guess a letter!");
      lv_label_set_text(_hint, "UP/DOWN letter   PRESS = guess");
      _refreshCells();
    }
    else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
  } else if (_state == ST_PLAY) {
    if      (dir == INavigation::DIR_UP)   _entry[_pos] = (_entry[_pos] == 'Z') ? 'A' : _entry[_pos] + 1;
    else if (dir == INavigation::DIR_DOWN) _entry[_pos] = (_entry[_pos] == 'A') ? 'Z' : _entry[_pos] - 1;
    else if (dir == INavigation::DIR_PRESS) { _guess(); return; }
    else if (dir == INavigation::DIR_BACK)  { Screen.goBack(); return; }
    _refreshCells();
  } else if (_state == ST_RESULT) {
    if      (dir == INavigation::DIR_PRESS) _resetSetup();
    else if (dir == INavigation::DIR_BACK)  Screen.goBack();
  }
}

void GameQuenswerScreen::onRender()
{
  // Live updates happen in onUpdate().
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#include "legacy_game_quenswer.inc"

#endif
