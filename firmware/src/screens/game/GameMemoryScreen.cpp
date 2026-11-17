//
// Memory Sequence — native LVGL build (USE_LVGL_SHELL). No sprites: the
// sequence is a row of rounded cells, input cycles a character with
// UP/DOWN, PRESS confirms, BACK removes. Difficulty, 3-mistake budget and
// stage multipliers preserved from the legacy game.
//

#include "GameMemoryScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#ifdef USE_LVGL_SHELL

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

  _arena = lv_obj_create(parent);
  lv_obj_set_pos(_arena, 0, 0);
  lv_obj_set_size(_arena, lv_obj_get_width(parent), lv_obj_get_height(parent));
  lv_obj_set_style_bg_color(_arena, lv_color_hex(0x0d1321), 0);
  lv_obj_set_style_bg_opa(_arena, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_arena, 0, 0);
  lv_obj_set_style_pad_all(_arena, 0, 0);
  lv_obj_set_style_radius(_arena, 0, 0);
  lv_obj_clear_flag(_arena, LV_OBJ_FLAG_SCROLLABLE);

  _title = lv_label_create(_arena);
  lv_obj_set_style_text_font(_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(_title, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_align(_title, LV_ALIGN_TOP_MID, 0, 6);
  lv_label_set_text(_title, "MEMORY");

  _status = lv_label_create(_arena);
  lv_obj_set_style_text_font(_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_status, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(_status, LV_ALIGN_TOP_MID, 0, 30);
  lv_label_set_text(_status, "");

  _board = lv_obj_create(_arena);
  lv_obj_align(_board, LV_ALIGN_CENTER, 0, -6);
  lv_obj_set_size(_board, 260, 46);
  lv_obj_set_style_bg_opa(_board, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_board, 0, 0);
  lv_obj_set_style_pad_all(_board, 0, 0);
  lv_obj_clear_flag(_board, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(_board, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(_board, 6, 0);

  for (int i = 0; i < kMaxSeq; i++) {
    _cells[i] = lv_label_create(_board);
    lv_obj_set_size(_cells[i], 32, 40);
    lv_obj_set_style_text_font(_cells[i], &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_opa(_cells[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_cells[i], lv_color_hex(0x1b2438), 0);
    lv_obj_set_style_radius(_cells[i], 6, 0);
    lv_obj_set_style_text_align(_cells[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(_cells[i], " ");
  }

  _hint = lv_label_create(_arena);
  lv_obj_set_style_text_font(_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_hint, lv_color_hex(0x6b7684), 0);
  lv_obj_align(_hint, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_label_set_text(_hint, "");
}

void GameMemoryScreen::_hideOverlay()
{
  if (_overlay) {
    lv_obj_del(_overlay);
    _overlay = nullptr;
    _ovN = 0;
  }
}

void GameMemoryScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _refreshBoard(false);
  lv_obj_add_flag(_board, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_hint, "");
  _menuIdx = 0;

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 220, 168);
  lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x101a26), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  const char* rows[kMenuN] = { "Play", nullptr, "Best", "Exit" };
  _ovN = kMenuN;
  for (int i = 0; i < kMenuN; i++) {
    _ovRows[i] = lv_label_create(_overlay);
    lv_obj_set_style_text_font(_ovRows[i], &lv_font_montserrat_16, 0);
    lv_obj_align(_ovRows[i], LV_ALIGN_TOP_MID, 0, i * 36);
    lv_label_set_text(_ovRows[i], rows[i] ? rows[i] : "");
  }
  lv_label_set_text(_status, "");
}

void GameMemoryScreen::_showScores()
{
  _state = ST_SCORES;
  _hideOverlay();
  lv_obj_add_flag(_board, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_hint, "any key = back");

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 230, 190);
  lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x101a26), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* t = lv_label_create(_overlay);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(t, lv_color_white(), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 2);
  lv_label_set_text(t, "BEST ROUNDS");

  for (int i = 0; i < kDiffN; i++) {
    lv_obj_t* r = lv_label_create(_overlay);
    lv_obj_set_style_text_font(r, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(r, lv_color_hex(0x9aa5b1), 0);
    lv_obj_align(r, LV_ALIGN_TOP_LEFT, 12, 34 + i * 30);
    char buf[40];
    snprintf(buf, sizeof(buf), "%-8s round %u", kDiffNames[i], _bestRound[i]);
    lv_label_set_text(r, buf);
  }
  _ovN = 0;
}

void GameMemoryScreen::_showLoss()
{
  _state = ST_LOSS;
  _hideOverlay();
  lv_obj_add_flag(_board, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_hint, "");

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 230, 150);
  lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x1a0d0d), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, lv_color_hex(0xb0540f), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* t = lv_label_create(_overlay);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0xff7043), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);
  lv_label_set_text(t, "GAME OVER");

  char buf[64];
  snprintf(buf, sizeof(buf), "score %lu   round %u", (unsigned long)_score, _round);
  lv_obj_t* s = lv_label_create(_overlay);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s, lv_color_white(), 0);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 44);
  lv_label_set_text(s, buf);

  if (_score > (uint32_t)_bestRound[_difficulty] * 10) ; // score kept, best tracked by round below

  lv_obj_t* h = lv_label_create(_overlay);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 74);
  lv_label_set_text(h, "PRESS = retry   BACK = menu");
  _ovN = 0;

  if (_round - 1 > _bestRound[_difficulty]) {
    _bestRound[_difficulty] = _round - 1;
    Config.set(String("mem_best_") + _difficulty, String(_bestRound[_difficulty]));
  }
}

void GameMemoryScreen::_newRound()
{
  const Stage& st = kStages[_difficulty];
  _seqLen = (uint8_t)min<int>(st.len + (_round - 1) / 2, kMaxSeq);
  for (int i = 0; i < _seqLen; i++) {
    const char* db = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    _seq[i] = db[random(36)];
  }
  _pos = 0;
  _cycleCh = 'A';

  char buf[64];
  snprintf(buf, sizeof(buf), "round %u    score %lu", _round, (unsigned long)_score);
  lv_label_set_text(_status, buf);
  lv_obj_clear_flag(_board, LV_OBJ_FLAG_HIDDEN);
  _refreshBoard(true);
  lv_label_set_text(_hint, "memorize…");

  _state = ST_SHOW;
  _showUntil = millis() + st.showMs + (uint32_t)(_seqLen * 120);
}

void GameMemoryScreen::_refreshBoard(bool reveal)
{
  for (int i = 0; i < kMaxSeq; i++) {
    if (i >= _seqLen) {
      lv_obj_add_flag(_cells[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(_cells[i], LV_OBJ_FLAG_HIDDEN);
    if (reveal) {
      lv_label_set_text(_cells[i], String(_seq[i]).c_str());
      lv_obj_set_style_bg_color(_cells[i], lv_color_hex(0x233252), 0);
      lv_obj_set_style_text_color(_cells[i], lv_color_white(), 0);
    } else {
      bool done = i < _pos;
      bool cur  = i == _pos;
      lv_label_set_text(_cells[i], done ? String(_seq[i]).c_str()
                                        : (cur ? String(_cycleCh).c_str() : " "));
      lv_obj_set_style_bg_color(_cells[i],
        done ? lv_color_hex(0x1e7a4b)
             : cur ? LvglUi::c565(Config.getThemeColor())
                   : lv_color_hex(0x1b2438), 0);
      lv_obj_set_style_text_color(_cells[i],
        done || cur ? lv_color_black() : lv_color_white(), 0);
    }
  }
}

void GameMemoryScreen::_confirm()
{
  if (_pos >= _seqLen) { _advancePhase(); return; }
  _pos++;
  if (_pos >= _seqLen) { _advancePhase(); return; }
  _cycleCh = 'A';
  _refreshBoard(false);
}

void GameMemoryScreen::_backspace()
{
  if (_pos > 0) _pos--;
  _refreshBoard(false);
}

void GameMemoryScreen::_advancePhase()
{
  if (_okSoFar) {
    _score += 10 * kStages[_difficulty].mult;
    _round++;
    lv_label_set_text(_hint, "correct!");
    _state = ST_OK;
    _flashUntil = millis() + 700;
  } else {
    _mistakes++;
    if (_mistakes >= 3) { _showLoss(); return; }
    lv_label_set_text(_hint, "mistake — lives left");
    _state = ST_BAD;
    _flashUntil = millis() + 700;
  }
}

void GameMemoryScreen::onUpdate()
{
  // Menu focus styling
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
    // dynamic difficulty row label
    char db[40];
    snprintf(db, sizeof(db), "Difficulty: %s", kDiffNames[_difficulty]);
    lv_label_set_text(_ovRows[1], db);
  }

  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();

    if (_state == ST_MENU) {
      if (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + _ovN - 1) % _ovN;
      if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % _ovN;
      if (dir == INavigation::DIR_PRESS) {
        if (_menuIdx == 0) { _round = 1; _score = 0; _mistakes = 0; _hideOverlay(); _newRound(); }
        else if (_menuIdx == 1) { _difficulty = (_difficulty + 1) % kDiffN; }
        else if (_menuIdx == 2) _showScores();
        else Screen.goBack();
      }
    } else if (_state == ST_SCORES) {
      if (dir != INavigation::DIR_NONE) _showMenu();
    } else if (_state == ST_LOSS) {
      if (dir == INavigation::DIR_PRESS) { _round = 1; _score = 0; _mistakes = 0; _hideOverlay(); _newRound(); }
      else if (dir == INavigation::DIR_BACK) _showMenu();
    } else if (_state == ST_INPUT) {
      if (dir == INavigation::DIR_UP)    { _cycleCh = (_cycleCh == 'A') ? '9' : (_cycleCh == 'Z') ? '0' : (_cycleCh == '9') ? 'A' : _cycleCh + 1; _refreshBoard(false); }
      if (dir == INavigation::DIR_DOWN) { _cycleCh = (_cycleCh == 'A') ? 'Z' : (_cycleCh == '0') ? '9' : (_cycleCh == '9') ? 'Z' : _cycleCh - 1; _refreshBoard(false); }
      if (dir == INavigation::DIR_PRESS) {
        // inline validation: compare immediately
        if (_cycleCh == _seq[_pos]) { _okSoFar = true; }
        else { _okSoFar = false; }
        _confirm();
      }
      if (dir == INavigation::DIR_BACK) _backspace();
    }
  }

  // Timed transitions
  uint32_t now = millis();
  if (_state == ST_SHOW && now >= _showUntil) {
    _state = ST_INPUT;
    _pos = 0;
    _cycleCh = 'A';
    _okSoFar = true;
    _refreshBoard(false);
    lv_label_set_text(_hint, "UP/DOWN = letter, PRESS = confirm");
  }
  if ((_state == ST_OK || _state == ST_BAD) && now >= _flashUntil) {
    if (_state == ST_OK) _newRound();
    else { _state = ST_INPUT; _refreshBoard(false); }
  }
}

void GameMemoryScreen::onRender()
{
  LvglUi::pump();
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
