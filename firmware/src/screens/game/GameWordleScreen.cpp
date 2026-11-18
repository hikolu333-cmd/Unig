//
// Wordle — native LVGL build (USE_LVGL_SHELL). 5×6 key-cap grid, stick input
// with held-repeat letter cycling. All languages EN/ID in one class.
//

#include "GameWordleScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "utils/wordle/en_common.h"
#include "utils/wordle/id_common.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)

void GameWordleScreen::onInit()
{
  _pickWord();
  _build();
}

void GameWordleScreen::_pickWord()
{
  const char (*db)[6] = (_lang == LANG_ID) ? WORDLE_DB_ID_COMMON : WORDLE_DB_EN_COMMON;
  int n = (_lang == LANG_ID)
    ? (int)(sizeof(WORDLE_DB_ID_COMMON) / sizeof(WORDLE_DB_ID_COMMON[0]))
    : (int)(sizeof(WORDLE_DB_EN_COMMON) / sizeof(WORDLE_DB_EN_COMMON[0]));
  if (n < 1) n = 1;
  memcpy(_target, db[random(n)], kWordLen);
  _target[kWordLen] = '\0';
}

void GameWordleScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();
  _arena = gamekit::arena(parent);

  constexpr int cellW = 40, cellH = 26, gap = 4;
  constexpr int gridW = kWordLen * cellW + (kWordLen - 1) * gap;   // 216
  constexpr int gridH = kMaxTries * cellH + (kMaxTries - 1) * gap; // 176
  constexpr int x0 = (320 - gridW) / 2;
  constexpr int y0 = 8;

  for (int r = 0; r < kMaxTries; r++) {
    for (int c = 0; c < kWordLen; c++) {
      _cells[r][c] = gamekit::cell(_arena, x0 + c * (cellW + gap),
                                    y0 + r * (cellH + gap),
                                    cellW, cellH, 0x2c3654, 6);
    }
  }

  _hint = gamekit::label(_arena, &lv_font_montserrat_14, 0xcfd8e3,
                         LV_ALIGN_BOTTOM_MID, 0, -2,
                         "UP/DOWN letter   LEFT/RIGHT move   PRESS = try");
  _refreshRow(0);
}

void GameWordleScreen::_refreshRow(int row)
{
  if (row >= kMaxTries) return;
  if (row > _row) {                       // future rows stay blank
    for (int c = 0; c < kWordLen; c++) {
      gamekit::setCellText(_cells[row][c], ' ');
      gamekit::paintCell(_cells[row][c], 0x2c3654);
      lv_obj_set_style_border_width(_cells[row][c], 1, 0);
    }
    return;
  }
  for (int c = 0; c < kWordLen; c++) {
    lv_obj_t* cl = _cells[row][c];
    bool cur = (row == _row && c == _col);
    gamekit::setCellText(cl, _entry[c]);
    gamekit::paintCell(cl, cur ? 0x3e5080 : 0x2c3654, 0xffffff,
                      cur ? 0xffd54f : 0x0d1321);
    lv_obj_set_style_border_width(cl, cur ? 2 : 1, 0);
  }
}

void GameWordleScreen::_markRow(int row)
{
  bool used[kWordLen] = {};
  for (int c = 0; c < kWordLen; c++) {
    if (_grid[row][c] == _target[c]) { _marks[row][c] = 2; used[c] = true; }
    else _marks[row][c] = 0;
  }
  for (int c = 0; c < kWordLen; c++) {
    if (_marks[row][c] == 2) continue;
    for (int t = 0; t < kWordLen; t++) {
      if (!used[t] && _grid[row][c] == _target[t]) {
        _marks[row][c] = 1; used[t] = true; break;
      }
    }
  }
  for (int c = 0; c < kWordLen; c++) {
    lv_obj_t* cl = _cells[row][c];
    gamekit::setCellText(cl, _grid[row][c]);
    switch (_marks[row][c]) {
      case 2:  gamekit::paintCell(cl, 0x43a047, 0x0d1321); break;
      case 1:  gamekit::paintCell(cl, 0xd4a017, 0x0d1321); break;
      default: gamekit::paintCell(cl, 0x55617a, 0xffffff); break;
    }
  }
}

void GameWordleScreen::_submitRow()
{
  if (_state != ST_PLAY) return;
  for (int c = 0; c < kWordLen; c++) _grid[_row][c] = _entry[c];
  _grid[_row][kWordLen] = '\0';
  _markRow(_row);

  if (strncmp(_grid[_row], _target, kWordLen) == 0) {
    if (Uni.Speaker) Uni.Speaker->playWin();
    const char* tag = (_lang == LANG_EN) ? "wordle_win_en" : "wordle_win_id";
    int n = Achievement.inc(tag);
    if (n == 1) Achievement.unlock(tag);
    _showEnd(true);
    return;
  }
  _row++;
  if (_row >= kMaxTries) {
    if (Uni.Speaker) Uni.Speaker->playLose();
    _showEnd(false);
    return;
  }
  _col = 0;
  _refreshRow(_row);
}

void GameWordleScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameWordleScreen::_showEnd(bool win)
{
  _state = ST_END;

  _overlay = gamekit::panel(_arena, 250, 150, win ? 0x24422e : 0x3a2a35);
  lv_obj_set_style_border_color(_overlay,
    lv_color_hex(win ? 0x43a047 : 0xff7043), 0);
  gamekit::label(_overlay, &lv_font_montserrat_20,
                 win ? 0x62d49c : 0xff7043, LV_ALIGN_TOP_MID, 0, 12,
                 win ? "SOLVED!" : "OUT OF TRIES");
  char buf[24];
  snprintf(buf, sizeof(buf), "word: %s", _target);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 52, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 92, "PRESS = again    BACK = exit");
}

void GameWordleScreen::_reset()
{
  _hideOverlay();
  _row = 0; _col = 0;
  memset(_grid, 0, sizeof(_grid));
  memset(_marks, 0, sizeof(_marks));
  memset(_entry, 'A', sizeof(_entry));
  _pickWord();
  for (int r = 0; r < kMaxTries; r++) _refreshRow(r);
  _state = ST_PLAY;
}

void GameWordleScreen::onUpdate()
{
  auto dir = _nav.poll();
  if (dir == INavigation::DIR_NONE) return;

  if (_state == ST_PLAY) {
    if      (dir == INavigation::DIR_UP)    _entry[_col] = (_entry[_col] == 'Z') ? 'A' : _entry[_col] + 1;
    else if (dir == INavigation::DIR_DOWN)  _entry[_col] = (_entry[_col] == 'A') ? 'Z' : _entry[_col] - 1;
    else if (dir == INavigation::DIR_RIGHT) { if (_col < kWordLen - 1) _col++; }
    else if (dir == INavigation::DIR_LEFT)  { if (_col > 0) _col--; }
    else if (dir == INavigation::DIR_BACK)  { Screen.goBack(); return; }
    else if (dir == INavigation::DIR_PRESS) { _submitRow(); return; }
    _refreshRow(_row);
  } else if (_state == ST_END) {
    if      (dir == INavigation::DIR_PRESS) _reset();
    else if (dir == INavigation::DIR_BACK)  Screen.goBack();
  }
}

void GameWordleScreen::onRender()
{
  // Live updates happen in onUpdate().
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────
#include "core/ConfigManager.h"
#include "screens/game/GameMenuScreen.h"

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "utils/wordle/en_common.h"
#include "utils/wordle/id_common.h"

#include "legacy_game_wordle.inc"

#endif
