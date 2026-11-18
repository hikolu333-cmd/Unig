//
// Memory Sequence — native LVGL build (USE_LVGL_SHELL). Key-cap cells, inline
// validation with green lock / red flash, GameNav held-repeat cycling.
//

#include "GameMemoryScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)

const char* const GameMemoryScreen::kDiffNames[kDiffN] = {
  "EASY", "MEDIUM", "HARD", "EXTREME"
};
constexpr GameMemoryScreen::Stage GameMemoryScreen::kStages[GameMemoryScreen::kDiffN];

void GameMemoryScreen::onInit()
{
  for (int i = 0; i < kDiffN; i++)
    _bestRound[i] = Config.get(String("mem_best_") + i, "0").toInt();
  _build();
  _showMenu();
}

void GameMemoryScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();
  _arena = gamekit::arena(parent);

  gamekit::label(_arena, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 4, "MEMORY");
  _status = gamekit::label(_arena, &lv_font_montserrat_14, 0xcfd8e3,
                           LV_ALIGN_TOP_MID, 0, 30, "");

  constexpr int cellW = 36, cellH = 44, gap = 6;
  constexpr int gridW = kMaxSeq * cellW + (kMaxSeq - 1) * gap;   // 288
  constexpr int x0 = (320 - gridW) / 2;
  constexpr int y0 = 56;
  for (int i = 0; i < kMaxSeq; i++) {
    _cells[i] = gamekit::cell(_arena, x0 + i * (cellW + gap), y0,
                              cellW, cellH, 0x2c3654, 8);
    lv_obj_set_style_text_font(_cells[i], &lv_font_montserrat_20, 0);
    lv_obj_add_flag(_cells[i], LV_OBJ_FLAG_HIDDEN);
  }

  _hint = gamekit::label(_arena, &lv_font_montserrat_14, 0xcfd8e3,
                         LV_ALIGN_BOTTOM_MID, 0, -2, "");
}

void GameMemoryScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameMemoryScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _menuIdx = 0;

  _overlay = gamekit::panel(_arena, 240, 186);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 8, "MEMORY SEQUENCE");
  const char* rows[4] = { "Play", "Difficulty", "Best", "Exit" };
  for (int i = 0; i < 4; i++)
    _ovRows[i] = gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                                LV_ALIGN_TOP_MID, 0, 42 + i * 32, rows[i]);

  for (int i = 0; i < kMaxSeq; i++) lv_obj_add_flag(_cells[i], LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_status, "");
  lv_label_set_text(_hint, "");
}

void GameMemoryScreen::_showScores()
{
  _state = ST_SCORES;
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 250, 170);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 8, "BEST ROUNDS");
  char buf[40];
  for (int i = 0; i < kDiffN; i++) {
    snprintf(buf, sizeof(buf), "%-9s round %u", kDiffNames[i], _bestRound[i]);
    gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                   LV_ALIGN_TOP_LEFT, 30, 48 + i * 28, buf);
  }
  lv_label_set_text(_hint, "any key = back");
}

void GameMemoryScreen::_showLoss()
{
  _state = ST_LOSS;
  _hideOverlay();

  if (_round - 1 > _bestRound[_difficulty]) {
    _bestRound[_difficulty] = _round - 1;
    Config.set(String("mem_best_") + _difficulty, String(_bestRound[_difficulty]));
  }

  _overlay = gamekit::panel(_arena, 250, 160, 0x3a2a35);
  lv_obj_set_style_border_color(_overlay, lv_color_hex(0xff7043), 0);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xff7043,
                 LV_ALIGN_TOP_MID, 0, 12, "GAME OVER");
  char buf[64];
  snprintf(buf, sizeof(buf), "score %lu   round %u", (unsigned long)_score, _round);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 52, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 100, "PRESS = retry    BACK = menu");
}

void GameMemoryScreen::_newRound()
{
  const Stage& st = kStages[_difficulty];
  _seqLen = (uint8_t)min<int>(st.len + (_round - 1) / 2, kMaxSeq);
  const char* db = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (int i = 0; i < _seqLen; i++) _seq[i] = db[random(36)];
  _seq[_seqLen] = '\0';
  _pos = 0;
  memset(_entry, 'A', sizeof(_entry));

  char buf[64];
  snprintf(buf, sizeof(buf), "round %u    score %lu    lives %d",
           _round, (unsigned long)_score, _lives);
  lv_label_set_text(_status, buf);
  lv_label_set_text(_hint, "memorize...");
  _refreshCells(true);

  _state = ST_SHOW;
  _showUntil = millis() + st.showMs + (uint32_t)(_seqLen * 140);
}

void GameMemoryScreen::_refreshCells(bool reveal)
{
  for (int i = 0; i < kMaxSeq; i++) {
    lv_obj_t* cl = _cells[i];
    if (i >= _seqLen) { lv_obj_add_flag(cl, LV_OBJ_FLAG_HIDDEN); continue; }
    lv_obj_clear_flag(cl, LV_OBJ_FLAG_HIDDEN);

    if (reveal) {
      gamekit::setCellText(cl, _seq[i]);
      gamekit::paintCell(cl, 0x3e5080, 0xffffff, 0xffd54f);
    } else {
      bool done = i < _pos;
      bool cur  = i == _pos;
      gamekit::setCellText(cl, done ? _seq[i] : _entry[i]);
      if (done)     gamekit::paintCell(cl, 0x43a047, 0x0d1321);
      else if (cur) gamekit::paintCell(cl, 0x3e5080, 0xffffff, 0xffd54f);
      else          gamekit::paintCell(cl, 0x2c3654, 0xffffff);
      lv_obj_set_style_border_width(cl, (done || cur) ? 2 : 1, 0);
    }
  }
}

void GameMemoryScreen::_confirmCell()
{
  if (_pos >= _seqLen) return;
  if (_entry[_pos] == _seq[_pos]) {
    _pos++;
    if (_pos >= _seqLen) {
      // round complete
      _score += 10 * kStages[_difficulty].mult;
      _round++;
      _state = ST_FLASH_OK;
      _flashUntil = millis() + 500;
      lv_label_set_text(_hint, "correct!");
      _refreshCells(false);
      // paint all locked
      for (int i = 0; i < _seqLen; i++) {
        gamekit::setCellText(_cells[i], _seq[i]);
        gamekit::paintCell(_cells[i], 0x43a047, 0x0d1321);
      }
      if (Uni.Speaker) Uni.Speaker->playRandomTone(0, 40);
    } else {
      _refreshCells(false);
    }
  } else {
    _lives--;
    _state = ST_FLASH_BAD;
    _flashUntil = millis() + 500;
    gamekit::paintCell(_cells[_pos], 0xd32f2f, 0xffffff);
    gamekit::setCellText(_cells[_pos], _entry[_pos]);
    char buf[64];
    snprintf(buf, sizeof(buf), "round %u    score %lu    lives %d",
             _round, (unsigned long)_score, _lives);
    lv_label_set_text(_status, buf);
    lv_label_set_text(_hint, "wrong!");
  }
}

void GameMemoryScreen::_menuFocus()
{
  if (_state != ST_MENU || !_overlay) return;
  for (int i = 0; i < 4; i++) gamekit::focusRow(_ovRows[i], i == _menuIdx);
  char db[40];
  snprintf(db, sizeof(db), "Difficulty: %s", kDiffNames[_difficulty]);
  lv_label_set_text(_ovRows[1], db);
}

void GameMemoryScreen::onUpdate()
{
  uint32_t now = millis();
  _menuFocus();

  if (_state == ST_SHOW && now >= _showUntil) {
    _state = ST_INPUT;
    _pos = 0;
    _refreshCells(false);
    lv_label_set_text(_hint, "UP/DOWN letter   PRESS = lock");
  }
  if (_state == ST_FLASH_OK && now >= _flashUntil) {
    char buf[64];
    snprintf(buf, sizeof(buf), "round %u    score %lu    lives %d",
             _round, (unsigned long)_score, _lives);
    lv_label_set_text(_status, buf);
    _newRound();
  }
  if (_state == ST_FLASH_BAD && now >= _flashUntil) {
    if (_lives == 0) { _showLoss(); }
    else { _state = ST_INPUT; _refreshCells(false); }
  }

  auto dir = _nav.poll();
  if (dir == INavigation::DIR_NONE) return;

  if (_state == ST_MENU) {
    if      (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + 3) % 4;
    else if (dir == INavigation::DIR_DOWN)  _menuIdx = (_menuIdx + 1) % 4;
    else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    else if (dir == INavigation::DIR_PRESS) {
      if      (_menuIdx == 0) { _round = 1; _score = 0; _lives = 3; _hideOverlay(); _newRound(); }
      else if (_menuIdx == 1) _difficulty = (_difficulty + 1) % kDiffN;
      else if (_menuIdx == 2) _showScores();
      else                   { Screen.goBack(); return; }
    }
  } else if (_state == ST_INPUT) {
    if      (dir == INavigation::DIR_UP)   _entry[_pos] = (_entry[_pos] == '9') ? 'A' : (_entry[_pos] == 'Z') ? '0' : _entry[_pos] + 1;
    else if (dir == INavigation::DIR_DOWN) _entry[_pos] = (_entry[_pos] == 'A') ? '9' : (_entry[_pos] == '0') ? 'Z' : _entry[_pos] - 1;
    else if (dir == INavigation::DIR_PRESS) { _confirmCell(); return; }
    else if (dir == INavigation::DIR_BACK)  { _showMenu(); return; }
    _refreshCells(false);
  } else if (_state == ST_SCORES) {
    _showMenu();
  } else if (_state == ST_LOSS) {
    if      (dir == INavigation::DIR_PRESS) { _round = 1; _score = 0; _lives = 3; _hideOverlay(); _newRound(); }
    else if (dir == INavigation::DIR_BACK)   _showMenu();
  }
}

void GameMemoryScreen::onRender()
{
  // Live updates happen in onUpdate().
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────
#include "core/ConfigManager.h"
#include "screens/game/GameMenuScreen.h"

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "screens/game/GameMenuScreen.h"

// full original implementation spliced from the repository HEAD
#include "legacy_game_memory.inc"

#endif
