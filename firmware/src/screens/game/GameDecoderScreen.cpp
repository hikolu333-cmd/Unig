//
// Decoder — native LVGL build (USE_LVGL_SHELL). Hex grid of rounded cells,
// countdown timer, Mastermind-style marks. Stick-only input.
//

#include "GameDecoderScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#ifdef USE_LVGL_SHELL

const char* const GameDecoderScreen::kDiffNames[kDiffN] = {
  "CALM", "RUSH", "PANIC"
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

  _arena = lv_obj_create(parent);
  lv_obj_set_pos(_arena, 0, 0);
  lv_obj_set_size(_arena, lv_obj_get_width(parent), lv_obj_get_height(parent));
  lv_obj_set_style_bg_color(_arena, lv_color_hex(0x0f1119), 0);
  lv_obj_set_style_bg_opa(_arena, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_arena, 0, 0);
  lv_obj_set_style_pad_all(_arena, 0, 0);
  lv_obj_set_style_radius(_arena, 0, 0);
  lv_obj_clear_flag(_arena, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* t = lv_label_create(_arena);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 4);
  lv_label_set_text(t, "DECODER");

  _timer = lv_label_create(_arena);
  lv_obj_set_style_text_font(_timer, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_timer, lv_color_hex(0xff7043), 0);
  lv_obj_align(_timer, LV_ALIGN_TOP_RIGHT, -8, 30);
  lv_label_set_text(_timer, "");

  const int cellW = 40, cellH = 40, gap = 6;
  const int gridW = kCodeLen * cellW + (kCodeLen - 1) * gap;
  const int x0 = (lv_obj_get_width(_arena) - gridW) / 2;
  const int y0 = 56;

  for (int r = 0; r < kMaxTries; r++) {
    for (int c = 0; c < kCodeLen; c++) {
      lv_obj_t* cell = lv_label_create(_arena);
      lv_obj_set_pos(cell, x0 + c * (cellW + gap), y0 + r * (cellH + gap));
      lv_obj_set_size(cell, cellW, cellH);
      lv_obj_set_style_text_font(cell, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(cell, lv_color_hex(0x1a1f2e), 0);
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
  lv_label_set_text(_hint, "UP/DOWN = digit   LEFT/RIGHT = move   PRESS = try");
}

void GameDecoderScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; _ovN = 0; }
}

void GameDecoderScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _menuIdx = 0;
  lv_label_set_text(_timer, "");

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 230, 150);
  lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x101a26), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  const char* rows[4] = { "Play", nullptr, "Best", "Exit" };
  _ovN = 4;
  for (int i = 0; i < 4; i++) {
    _ovRows[i] = lv_label_create(_overlay);
    lv_obj_set_style_text_font(_ovRows[i], &lv_font_montserrat_16, 0);
    lv_obj_align(_ovRows[i], LV_ALIGN_TOP_MID, 0, i * 32);
    lv_label_set_text(_ovRows[i], rows[i] ? rows[i] : "");
  }
}

void GameDecoderScreen::_showResult(bool win)
{
  _state = ST_RESULT;
  _hideOverlay();

  if (win) {
    uint8_t solved = _row + 1;
    if (solved > _best[_diff]) {
      _best[_diff] = solved;
      Config.set(String("dec_best_") + _diff, String(solved));
    }
    int n = Achievement.inc("decoder_win");
    if (n == 1) Achievement.unlock("decoder_win");
  }

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 240, 140);
  lv_obj_set_style_bg_color(_overlay, win ? lv_color_hex(0x0d1a12) : lv_color_hex(0x1a0d0d), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, win ? lv_color_hex(0x2e8b57) : lv_color_hex(0xb0540f), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* t = lv_label_create(_overlay);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, win ? lv_color_hex(0x62d49c) : lv_color_hex(0xff7043), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);
  lv_label_set_text(t, win ? "CRACKED!" : "TIME OUT");

  char buf[32];
  snprintf(buf, sizeof(buf), "code was %s", _secret);
  lv_obj_t* s = lv_label_create(_overlay);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s, lv_color_white(), 0);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 42);
  lv_label_set_text(s, buf);

  lv_obj_t* h = lv_label_create(_overlay);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 70);
  lv_label_set_text(h, "PRESS = retry   BACK = menu");
  _ovN = 0;
}

void GameDecoderScreen::_startPlay()
{
  _hideOverlay();
  const char* hex = "0123456789ABCDEF";
  for (int i = 0; i < kCodeLen; i++) _secret[i] = hex[random(16)];
  _secret[kCodeLen] = '\0';
  _row = 0; _col = 0; _digit = 0;
  memset(_entry, 0, sizeof(_entry));
  _endMs = millis() + kTimeMs[_diff];
  _state = ST_PLAY;
  for (int r = 0; r < kMaxTries; r++) _refreshRow(r);
  _refreshRow(0);
}

void GameDecoderScreen::_refreshRow(uint8_t row)
{
  for (int c = 0; c < kCodeLen; c++) {
    lv_obj_t* cell = _cells[row][c];
    if (row < _row) continue;   // submitted rows keep marks
    bool cur = (c == _col);
    char buf[2] = { "0123456789ABCDEF"[_entry[c]], 0 };
    lv_label_set_text(cell, buf);
    lv_obj_set_style_bg_color(cell, cur ? lv_color_hex(0x2a3654) : lv_color_hex(0x1a1f2e), 0);
    lv_obj_set_style_border_color(cell,
      cur ? LvglUi::c565(Config.getThemeColor()) : lv_color_hex(0x2c3650), 0);
    lv_obj_set_style_text_color(cell, lv_color_white(), 0);
  }
}

void GameDecoderScreen::_markRow(uint8_t row)
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
  for (int c = 0; c < kCodeLen; c++) {
    lv_obj_t* cell = _cells[row][c];
    switch (_marks[row][c]) {
      case 2:  lv_obj_set_style_bg_color(cell, lv_color_hex(0x2e8b57), 0);
               lv_obj_set_style_text_color(cell, lv_color_black(), 0); break;
      case 1:  lv_obj_set_style_bg_color(cell, lv_color_hex(0xd4a017), 0);
               lv_obj_set_style_text_color(cell, lv_color_black(), 0); break;
      default: lv_obj_set_style_bg_color(cell, lv_color_hex(0x22283a), 0);
               lv_obj_set_style_text_color(cell, lv_color_hex(0x8a94a6), 0); break;
    }
  }
}

void GameDecoderScreen::_submitRow()
{
  for (int c = 0; c < kCodeLen; c++) _grid[_row][c] = "0123456789ABCDEF"[_entry[c]];
  _grid[_row][kCodeLen] = '\0';
  _markRow(_row);

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
  // menu focus
  if (_state == ST_MENU && _overlay) {
    for (int i = 0; i < _ovN; i++) {
      bool sel = (i == _menuIdx);
      lv_obj_set_style_text_color(_ovRows[i], sel ? lv_color_black() : lv_color_white(), 0);
      lv_obj_set_style_bg_color(_ovRows[i],
        sel ? LvglUi::c565(Config.getThemeColor()) : lv_color_hex(0x101a26), 0);
      lv_obj_set_style_bg_opa(_ovRows[i], LV_OPA_COVER, 0);
      lv_obj_set_style_pad_hor(_ovRows[i], 14, 0);
      lv_obj_set_style_pad_ver(_ovRows[i], 5, 0);
      lv_obj_set_style_radius(_ovRows[i], 5, 0);
    }
    char db[36];
    snprintf(db, sizeof(db), "Difficulty: %s", kDiffNames[_diff]);
    lv_label_set_text(_ovRows[1], db);
  }

  // countdown
  if (_state == ST_PLAY) {
    int left = (int)(_endMs - millis());
    char tb[16];
    snprintf(tb, sizeof(tb), "%ds", max(0, left / 1000));
    lv_label_set_text(_timer, tb);
    if (left <= 0) { if (Uni.Speaker) Uni.Speaker->playLose(); _showResult(false); }
  }

  if (!Uni.Nav->wasPressed()) return;
  auto dir = Uni.Nav->readDirection();

  if (_state == ST_MENU) {
    if (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + _ovN - 1) % _ovN;
    if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % _ovN;
    if (dir == INavigation::DIR_PRESS) {
      if (_menuIdx == 0) _startPlay();
      else if (_menuIdx == 1) _diff = (_diff + 1) % kDiffN;
      else Screen.goBack();
    }
  } else if (_state == ST_PLAY) {
    if      (dir == INavigation::DIR_UP)    { _digit = (_digit + 1) & 0xF; _entry[_col] = _digit; }
    else if (dir == INavigation::DIR_DOWN) { _digit = (_digit + 15) & 0xF; _entry[_col] = _digit; }
    else if (dir == INavigation::DIR_RIGHT) { if (_col < kCodeLen - 1) { _col++; _digit = _entry[_col]; } }
    else if (dir == INavigation::DIR_LEFT || dir == INavigation::DIR_BACK) {
      if (_col > 0) { _col--; _digit = _entry[_col]; }
    }
    else if (dir == INavigation::DIR_PRESS) { _submitRow(); return; }
    _refreshRow(_row);
  } else if (_state == ST_RESULT) {
    if (dir == INavigation::DIR_PRESS) _startPlay();
    else if (dir == INavigation::DIR_BACK) _showMenu();
  }
}

void GameDecoderScreen::onRender()
{
  LvglUi::pump();
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#include "legacy_game_decoder.inc"

#endif
