//
// Decoder — native LVGL build (USE_LVGL_SHELL). Hex grid of key-cap cells,
// live countdown, Mastermind marks. All input through GameNav (held-repeat).
//

#include "GameDecoderScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)

const char* const GameDecoderScreen::kDiffNames[kDiffN] = {
  "CALM 90s", "RUSH 60s", "PANIC 30s"
};
constexpr uint32_t GameDecoderScreen::kTimeMs[GameDecoderScreen::kDiffN];

void GameDecoderScreen::onInit()
{
  for (int i = 0; i < kDiffN; i++)
    _best[i] = Config.get(String("dec_best_") + i, "0").toInt();
  _build();
  _showMenu();
}

void GameDecoderScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();
  _arena = gamekit::arena(parent);

  gamekit::label(_arena, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_LEFT, 10, 4, "DECODER");
  _timer = gamekit::label(_arena, &lv_font_montserrat_16, 0xff7043,
                          LV_ALIGN_TOP_RIGHT, -10, 6, "");

  constexpr int cellW = 36, cellH = 19, gap = 3;
  constexpr int gridW = kCodeLen * cellW + (kCodeLen - 1) * gap;   // 153
  constexpr int gridH = kMaxTries * cellH + (kMaxTries - 1) * gap; // 173
  constexpr int x0 = (320 - gridW) / 2;
  constexpr int y0 = 30;

  for (int r = 0; r < kMaxTries; r++) {
    for (int c = 0; c < kCodeLen; c++) {
      _cells[r][c] = gamekit::cell(_arena, x0 + c * (cellW + gap),
                                   y0 + r * (cellH + gap),
                                   cellW, cellH, 0x2c3654, 4);
      lv_obj_set_style_text_font(_cells[r][c], &lv_font_montserrat_14, 0);
    }
  }
}

void GameDecoderScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameDecoderScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _menuIdx = 0;
  lv_label_set_text(_timer, "");

  _overlay = gamekit::panel(_arena, 240, 186);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 8, "DECODER");
  const char* rows[4] = { "Play", "Difficulty", "Best", "Exit" };
  for (int i = 0; i < 4; i++)
    _ovRows[i] = gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                                LV_ALIGN_TOP_MID, 0, 42 + i * 32, rows[i]);
}

void GameDecoderScreen::_showBest()
{
  _state = ST_MENU;           // same key handling as the main menu
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 240, 170);
  _ovRows[0] = _ovRows[1] = _ovRows[2] = _ovRows[3] = nullptr;
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 8, "BEST (codes cracked)");
  char buf[40];
  for (int i = 0; i < kDiffN; i++) {
    snprintf(buf, sizeof(buf), "%-8s %d", kDiffNames[i], _best[i]);
    gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                   LV_ALIGN_TOP_LEFT, 24, 46 + i * 30, buf);
  }
}

void GameDecoderScreen::_showResult(bool win)
{
  _state = ST_RESULT;
  _hideOverlay();

  if (win) {
    int solved = _row + 1;
    if (solved > _best[_diff]) {
      _best[_diff] = solved;
      Config.set(String("dec_best_") + _diff, String(solved));
    }
    int n = Achievement.inc("decoder_win");
    if (n == 1) Achievement.unlock("decoder_win");
  }

  _overlay = gamekit::panel(_arena, 240, 150, win ? 0x24422e : 0x3a2a35);
  lv_obj_set_style_border_color(_overlay,
    lv_color_hex(win ? 0x43a047 : 0xff7043), 0);
  gamekit::label(_overlay, &lv_font_montserrat_20,
                 win ? 0x62d49c : 0xff7043, LV_ALIGN_TOP_MID, 0, 12,
                 win ? "CRACKED!" : "TIME OUT");
  char buf[32];
  snprintf(buf, sizeof(buf), "code was %s", _secret);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 52, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 92, "PRESS = retry    BACK = menu");
}

void GameDecoderScreen::_startPlay()
{
  _hideOverlay();
  const char* digits = "0123456789";
  for (int i = 0; i < kCodeLen; i++) _secret[i] = digits[random(10)];
  _secret[kCodeLen] = '\0';
  _row = 0; _col = 0;
  memset(_entry, 0, sizeof(_entry));
  memset(_grid, 0, sizeof(_grid));
  _endMs = millis() + kTimeMs[_diff];
  _state = ST_PLAY;

  for (int r = 0; r < kMaxTries; r++) {
    for (int c = 0; c < kCodeLen; c++) {
      gamekit::setCellText(_cells[r][c], ' ');
      gamekit::paintCell(_cells[r][c], 0x2c3654);
    }
  }
  _refreshRow(0);
}

void GameDecoderScreen::_paintCell(int r, int c)
{
  lv_obj_t* cl = _cells[r][c];
  if (r < _row) {                                     // submitted row → marks
    switch (_marks[r][c]) {
      case 2:  gamekit::paintCell(cl, 0x43a047, 0x0d1321); break;
      case 1:  gamekit::paintCell(cl, 0xd4a017, 0x0d1321); break;
      default: gamekit::paintCell(cl, 0x55617a, 0xffffff); break;
    }
    return;
  }
  bool cur = (r == _row && c == _col);
  gamekit::paintCell(cl, cur ? 0x3e5080 : 0x2c3654, 0xffffff,
                    cur ? 0xffd54f : 0x0d1321);
  if (cur) lv_obj_set_style_border_width(cl, 2, 0);
  else     lv_obj_set_style_border_width(cl, 1, 0);
}

void GameDecoderScreen::_refreshRow(int row)
{
  for (int c = 0; c < kCodeLen; c++) {
    gamekit::setCellText(_cells[row][c],
      row == _row ? "0123456789"[_entry[c]] : ' ');
    _paintCell(row, c);
  }
}

void GameDecoderScreen::_markRow(int row)
{
  bool used[kCodeLen] = {};
  for (int c = 0; c < kCodeLen; c++) {
    if (_grid[row][c] == _secret[c]) { _marks[row][c] = 2; used[c] = true; }
    else _marks[row][c] = 0;
  }
  for (int c = 0; c < kCodeLen; c++) {
    if (_marks[row][c] == 2) continue;
    for (int t = 0; t < kCodeLen; t++) {
      if (!used[t] && _grid[row][c] == _secret[t]) {
        _marks[row][c] = 1; used[t] = true; break;
      }
    }
  }
}

void GameDecoderScreen::_submitRow()
{
  for (int c = 0; c < kCodeLen; c++) _grid[_row][c] = "0123456789"[_entry[c]];
  _grid[_row][kCodeLen] = '\0';
  _markRow(_row);
  for (int c = 0; c < kCodeLen; c++) _paintCell(_row, c);

  if (strncmp(_grid[_row], _secret, kCodeLen) == 0) {
    if (Uni.Speaker) Uni.Speaker->playWin();
    _showResult(true);
    return;
  }
  _row++;
  if (_row >= kMaxTries) {
    if (Uni.Speaker) Uni.Speaker->playLose();
    _showResult(false);
    return;
  }
  _col = 0;
  _refreshRow(_row);
}

void GameDecoderScreen::onUpdate()
{
  // menu focus styling
  if (_state == ST_MENU && _overlay) {
    for (int i = 0; i < 4; i++) {
      if (!_ovRows[i]) continue;
      gamekit::focusRow(_ovRows[i], i == _menuIdx);
    }
    if (_ovRows[1]) {
      char db[36];
      snprintf(db, sizeof(db), "Difficulty: %s", kDiffNames[_diff]);
      lv_label_set_text(_ovRows[1], db);
    }
  }

  // countdown
  if (_state == ST_PLAY) {
    int left = (int)(_endMs - millis());
    char tb[16];
    snprintf(tb, sizeof(tb), "%ds", max(0, left / 1000));
    lv_label_set_text(_timer, tb);
    if (left <= 0) {
      if (Uni.Speaker) Uni.Speaker->playLose();
      _showResult(false);
    }
  }

  auto dir = _nav.poll();
  if (dir == INavigation::DIR_NONE) return;

  if (_state == ST_MENU) {
    if      (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + 3) % 4;
    else if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % 4;
    else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    else if (dir == INavigation::DIR_PRESS) {
      if (!_ovRows[0]) { _showMenu(); return; }        // best view → main menu
      if      (_menuIdx == 0) _startPlay();
      else if (_menuIdx == 1) _diff = (_diff + 1) % kDiffN;
      else if (_menuIdx == 2) _showBest();
      else                   { Screen.goBack(); return; }
    }
  } else if (_state == ST_PLAY) {
    if      (dir == INavigation::DIR_UP)    { _entry[_col] = (_entry[_col] + 1) % 10; }
    else if (dir == INavigation::DIR_DOWN)  { _entry[_col] = (_entry[_col] + 9) % 10; }
    else if (dir == INavigation::DIR_RIGHT) { if (_col < kCodeLen - 1) _col++; }
    else if (dir == INavigation::DIR_LEFT)  { if (_col > 0) _col--; }
    else if (dir == INavigation::DIR_BACK)  { _showMenu(); return; }
    else if (dir == INavigation::DIR_PRESS) { _submitRow(); return; }
    _refreshRow(_row);
  } else if (_state == ST_RESULT) {
    if      (dir == INavigation::DIR_PRESS) _startPlay();
    else if (dir == INavigation::DIR_BACK)  _showMenu();
  }
}

void GameDecoderScreen::onRender()
{
  // Live updates happen in onUpdate(); nothing to draw per-frame here.
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#include "legacy_game_decoder.inc"

#endif
