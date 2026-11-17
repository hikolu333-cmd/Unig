//
// Wordle — native LVGL build (USE_LVGL_SHELL). Letter grid as rounded label
// cells; stick-only input (no keyboard): UP/DOWN cycle the letter, RIGHT next
// slot, LEFT previous, PRESS submit, BACK delete. Standard marking rules.
//

#include "GameWordleScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "utils/wordle/en_common.h"
#include "utils/wordle/id_common.h"

#ifdef USE_LVGL_SHELL

void GameWordleScreen::onInit()
{
  _pickWord();
  _build();
}

void GameWordleScreen::_pickWord()
{
  const char (*db)[6] = (_lang == LANG_ID) ? WORDLE_DB_ID_COMMON : WORDLE_DB_EN_COMMON;
  uint16_t n = (uint16_t)(sizeof(WORDLE_DB_ID_COMMON) / sizeof(WORDLE_DB_ID_COMMON[0]));
  if (_lang == LANG_EN)
    n = (uint16_t)(sizeof(WORDLE_DB_EN_COMMON) / sizeof(WORDLE_DB_EN_COMMON[0]));
  _seed = (uint32_t)random(1000000);
  memcpy(_target, db[_seed % n], kWordLen);
  _target[kWordLen] = '\0';
}

void GameWordleScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();

  _arena = lv_obj_create(parent);
  lv_obj_set_pos(_arena, 0, 0);
  lv_obj_set_size(_arena, lv_obj_get_width(parent), lv_obj_get_height(parent));
  lv_obj_set_style_bg_color(_arena, lv_color_hex(0x12131f), 0);
  lv_obj_set_style_bg_opa(_arena, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_arena, 0, 0);
  lv_obj_set_style_pad_all(_arena, 0, 0);
  lv_obj_set_style_radius(_arena, 0, 0);
  lv_obj_clear_flag(_arena, LV_OBJ_FLAG_SCROLLABLE);

  const int cellW = 34, cellH = 38, gap = 6;
  const int gridW = kWordLen * cellW + (kWordLen - 1) * gap;
  const int gridH = kMaxTries * cellH + (kMaxTries - 1) * gap;
  const int x0 = (lv_obj_get_width(_arena) - gridW) / 2;
  const int y0 = (lv_obj_get_height(_arena) - gridH) / 2 - 8;

  for (int r = 0; r < kMaxTries; r++) {
    for (int c = 0; c < kWordLen; c++) {
      lv_obj_t* cell = lv_label_create(_arena);
      lv_obj_set_pos(cell, x0 + c * (cellW + gap), y0 + r * (cellH + gap));
      lv_obj_set_size(cell, cellW, cellH);
      lv_obj_set_style_text_font(cell, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(cell, lv_color_hex(0x1b2438), 0);
      lv_obj_set_style_border_color(cell, lv_color_hex(0x2c3650), 0);
      lv_obj_set_style_border_width(cell, 1, 0);
      lv_obj_set_style_radius(cell, 6, 0);
      lv_label_set_text(cell, " ");
      _cells[r][c] = cell;
    }
  }

  _hint = lv_label_create(_arena);
  lv_obj_set_style_text_font(_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_hint, lv_color_hex(0x7a8496), 0);
  lv_obj_align(_hint, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_label_set_text(_hint, "UP/DOWN = letter   LEFT/RIGHT = move   PRESS = try");

  _row = 0; _col = 0; _letter = 'A';
  _refreshRow(0);
}

void GameWordleScreen::_cycle(int dir)
{
  char l = _letter;
  do {
    l = (dir > 0) ? (l == 'Z' ? 'A' : l + 1)
                  : (l == 'A' ? 'Z' : l - 1);
  } while (_lang == LANG_ID &&
           !(l == 'A' || l == 'B' || l == 'C' || l == 'D' || l == 'E' || l == 'F' ||
             l == 'G' || l == 'H' || l == 'I' || l == 'J' || l == 'K' || l == 'L' ||
             l == 'M' || l == 'N' || l == 'O' || l == 'P' || l == 'R' || l == 'S' ||
             l == 'T' || l == 'U' || l == 'V' || l == 'W' || l == 'Y'));
  _letter = l;
  _entry[_col] = l;
  _refreshRow(_row);
}

void GameWordleScreen::_refreshRow(uint8_t row)
{
  for (int c = 0; c < kWordLen; c++) {
    lv_obj_t* cell = _cells[row][c];
    if (row < _row) continue;                    // submitted rows keep their marks
    bool isCursor = (c == _col);
    char ch = isCursor ? _letter : _entry[c];
    char buf[2] = { ch, 0 };
    lv_label_set_text(cell, buf);
    lv_obj_set_style_bg_color(cell,
      isCursor ? lv_color_hex(0x2a3654) : lv_color_hex(0x1b2438), 0);
    lv_obj_set_style_border_color(cell,
      isCursor ? LvglUi::c565(Config.getThemeColor()) : lv_color_hex(0x2c3650), 0);
    lv_obj_set_style_text_color(cell, lv_color_white(), 0);
  }
}

void GameWordleScreen::_markRow(uint8_t row)
{
  // Standard Wordle marking with a consumed-letter pass for the yellows.
  bool used[kWordLen] = {};
  for (int c = 0; c < kWordLen; c++) {
    if (_grid[row][c] == _target[c]) { _marks[row][c] = 2; used[c] = true; }
    else _marks[row][c] = 0;
  }
  for (int c = 0; c < kWordLen; c++) {
    if (_marks[row][c] == 2) continue;
    for (int t = 0; t < kWordLen; t++) {
      if (!used[t] && _grid[row][c] == _target[t]) {
        _marks[row][c] = 1;
        used[t] = true;
        break;
      }
    }
  }
  for (int c = 0; c < kWordLen; c++) {
    lv_obj_t* cell = _cells[row][c];
    char buf[2] = { (char)(_grid[row][c] - ('a' - 'A')), 0 };
    lv_label_set_text(cell, buf);
    switch (_marks[row][c]) {
      case 2:  lv_obj_set_style_bg_color(cell, lv_color_hex(0x2e8b57), 0);
               lv_obj_set_style_text_color(cell, lv_color_black(), 0); break;
      case 1:  lv_obj_set_style_bg_color(cell, lv_color_hex(0xd4a017), 0);
               lv_obj_set_style_text_color(cell, lv_color_black(), 0); break;
      default: lv_obj_set_style_bg_color(cell, lv_color_hex(0x242a3a), 0);
               lv_obj_set_style_text_color(cell, lv_color_hex(0x8a94a6), 0); break;
    }
    lv_obj_set_style_border_color(cell, lv_color_hex(0x2c3650), 0);
  }
}

void GameWordleScreen::_submitRow()
{
  if (_state != ST_PLAY) return;
  for (int c = 0; c < kWordLen; c++) _grid[_row][c] = (char)(_entry[c] - 'A' + 'a');
  _grid[_row][kWordLen] = '\0';
  // (all five letters were chosen explicitly — no blanks possible)

  _markRow(_row);

  bool win = strncmp(_grid[_row], _target, kWordLen) == 0;
  if (win) {
    if (Uni.Speaker) Uni.Speaker->playWin();
    int n = Achievement.inc((String("wordle_win_") + (_lang == LANG_EN ? "en" : "id")).c_str());
    if (n == 1) Achievement.unlock((String("wordle_win_") + (_lang == LANG_EN ? "en" : "id")).c_str());
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
  _state = win ? ST_WIN : ST_LOSS;

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 240, 130);
  lv_obj_set_style_bg_color(_overlay, win ? lv_color_hex(0x0d1a12) : lv_color_hex(0x1a0d0d), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, win ? lv_color_hex(0x2e8b57) : lv_color_hex(0xb0540f), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  _ovTitle = lv_label_create(_overlay);
  lv_obj_set_style_text_font(_ovTitle, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(_ovTitle, win ? lv_color_hex(0x62d49c) : lv_color_hex(0xff7043), 0);
  lv_obj_align(_ovTitle, LV_ALIGN_TOP_MID, 0, 10);
  lv_label_set_text(_ovTitle, win ? "SOLVED!" : "OUT OF TRIES");

  char buf[24];
  snprintf(buf, sizeof(buf), "word: %s", _target);
  _ovSub = lv_label_create(_overlay);
  lv_obj_set_style_text_font(_ovSub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_ovSub, lv_color_white(), 0);
  lv_obj_align(_ovSub, LV_ALIGN_TOP_MID, 0, 44);
  lv_label_set_text(_ovSub, buf);

  lv_obj_t* h = lv_label_create(_overlay);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 72);
  lv_label_set_text(h, "PRESS = again   BACK = exit");
}

void GameWordleScreen::onUpdate()
{
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();

    if (_state == ST_PLAY) {
      if      (dir == INavigation::DIR_UP)    _cycle(1);
      else if (dir == INavigation::DIR_DOWN) _cycle(-1);
      else if (dir == INavigation::DIR_RIGHT) { if (_col < kWordLen - 1) { _col++; _letter = _entry[_col]; _refreshRow(_row); } }
      else if (dir == INavigation::DIR_LEFT)  { if (_col > 0)           { _col--; _letter = _entry[_col]; _refreshRow(_row); } }
      else if (dir == INavigation::DIR_BACK)  { if (_col > 0)          { _col--; _letter = _entry[_col]; _refreshRow(_row); } }
      else if (dir == INavigation::DIR_PRESS) _submitRow();
    } else {
      if (dir == INavigation::DIR_PRESS) {
        // fresh game
        _hideOverlay();
        _row = 0; _col = 0; _letter = 'A'; _state = ST_PLAY;
        memset(_grid, 0, sizeof(_grid));
        memset(_entry, 'A', sizeof(_entry));
        _pickWord();
        for (int r = 0; r < kMaxTries; r++) _refreshRow(r);
      } else if (dir == INavigation::DIR_BACK) {
        Screen.goBack();
        return;
      }
    }
  }
}

void GameWordleScreen::onRender()
{
  LvglUi::pump();
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
