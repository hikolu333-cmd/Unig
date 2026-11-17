//
// Number Guess — native LVGL build (USE_LVGL_SHELL). No sprites: value label
// in the middle, hint below, tries counter top-right. Menu/result as panels.
//

#include "GameNumberGuessScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#ifdef USE_LVGL_SHELL

const char* const GameNumberGuessScreen::kDiffNames[kDiffN] = {
  "EASY", "MEDIUM", "HARD", "EXTREME"
};

constexpr int GameNumberGuessScreen::kMaxVal[GameNumberGuessScreen::kDiffN];
constexpr int GameNumberGuessScreen::kTrials[GameNumberGuessScreen::kDiffN];

void GameNumberGuessScreen::onInit()
{
  for (int i = 0; i < kDiffN; i++)
    _best[i] = Config.get(String("ng_best_") + i, "999").toInt();
  _build();
  _showMenu();
}

void GameNumberGuessScreen::_build()
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

  lv_obj_t* t = lv_label_create(_arena);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
  lv_label_set_text(t, "GUESS THE NUMBER");

  _triesLbl = lv_label_create(_arena);
  lv_obj_set_style_text_font(_triesLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_triesLbl, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(_triesLbl, LV_ALIGN_TOP_RIGHT, -8, 32);
  lv_label_set_text(_triesLbl, "");

  _valueLbl = lv_label_create(_arena);
  lv_obj_set_style_text_font(_valueLbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(_valueLbl, lv_color_white(), 0);
  lv_obj_set_style_bg_color(_valueLbl, lv_color_hex(0x1b2438), 0);
  lv_obj_set_style_bg_opa(_valueLbl, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(_valueLbl, 22, 0);
  lv_obj_set_style_pad_ver(_valueLbl, 10, 0);
  lv_obj_set_style_radius(_valueLbl, 8, 0);
  lv_obj_align(_valueLbl, LV_ALIGN_CENTER, 0, -14);
  lv_label_set_text(_valueLbl, "0");

  _hintLbl = lv_label_create(_arena);
  lv_obj_set_style_text_font(_hintLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_hintLbl, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(_hintLbl, LV_ALIGN_CENTER, 0, 26);
  lv_label_set_text(_hintLbl, "");

  lv_obj_t* h = lv_label_create(_arena);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x5b6673), 0);
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_label_set_text(h, "UP/DOWN = ±1   LEFT/RIGHT = ±10   PRESS = go");
}

void GameNumberGuessScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; _ovN = 0; }
}

void GameNumberGuessScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _menuIdx = 0;

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 230, 170);
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
  lv_label_set_text(_triesLbl, "");
  lv_label_set_text(_hintLbl, "");
}

void GameNumberGuessScreen::_showScores()
{
  _state = ST_SCORES;
  _hideOverlay();

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
  lv_label_set_text(t, "BEST (fewest tries)");

  for (int i = 0; i < kDiffN; i++) {
    lv_obj_t* r = lv_label_create(_overlay);
    lv_obj_set_style_text_font(r, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(r, lv_color_hex(0x9aa5b1), 0);
    lv_obj_align(r, LV_ALIGN_TOP_LEFT, 14, 34 + i * 30);
    char buf[40];
    bool played = _best[i] < 900;
    snprintf(buf, sizeof(buf), "%-8s %s", kDiffNames[i], played ? String(_best[i]).c_str() : "—");
    lv_label_set_text(r, buf);
  }
  _ovN = 0;
}

void GameNumberGuessScreen::_showResult(bool win)
{
  _state = ST_RESULT;
  _hideOverlay();

  if (win && (_best[_diff] > _tries || _best[_diff] >= 900)) {
    _best[_diff] = _tries;
    Config.set(String("ng_best_") + _diff, String(_tries));
  }

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 230, 150);
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
  lv_label_set_text(t, win ? "CORRECT!" : "OUT OF TRIES");

  char buf[56];
  snprintf(buf, sizeof(buf), "the number was %d   tries %d", _target, _tries);
  lv_obj_t* s = lv_label_create(_overlay);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s, lv_color_white(), 0);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 44);
  lv_label_set_text(s, buf);

  lv_obj_t* h = lv_label_create(_overlay);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 74);
  lv_label_set_text(h, "PRESS = retry   BACK = menu");
  _ovN = 0;
}

void GameNumberGuessScreen::_startPlay()
{
  _hideOverlay();
  _target = random(0, kMaxVal[_diff] + 1);
  _guess  = kMaxVal[_diff] / 2;
  _tries  = 0;
  _state  = ST_PLAY;
  _paintValue();
  lv_label_set_text(_triesLbl, "tries 0");
  lv_label_set_text(_hintLbl, "make a guess…");
}

void GameNumberGuessScreen::_paintValue()
{
  lv_label_set_text(_valueLbl, String(_guess).c_str());
}

void GameNumberGuessScreen::_submit()
{
  _tries++;
  lv_label_set_text(_triesLbl, (String("tries ") + _tries).c_str());
  if (_guess == _target) {
    if (Uni.Speaker) Uni.Speaker->playWin();
    int n = Achievement.inc("numguess_first_win");
    if (n == 1) Achievement.unlock("numguess_first_win");
    _showResult(true);
    return;
  }
  _lastHigh = _target > _guess;
  lv_label_set_text(_hintLbl, _lastHigh ? "⌃ higher" : "⌄ lower");
  lv_obj_set_style_text_color(_hintLbl,
    _lastHigh ? lv_color_hex(0x62d49c) : lv_color_hex(0xffb380), 0);
  if (kTrials[_diff] > 0 && _tries >= kTrials[_diff]) _showResult(false);
}

void GameNumberGuessScreen::onUpdate()
{
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
    char db[40];
    snprintf(db, sizeof(db), "Difficulty: %s", kDiffNames[_diff]);
    lv_label_set_text(_ovRows[1], db);
  }

  if (!Uni.Nav->wasPressed()) return;
  auto dir = Uni.Nav->readDirection();

  if (_state == ST_MENU) {
    if (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + _ovN - 1) % _ovN;
    if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % _ovN;
    if (dir == INavigation::DIR_PRESS) {
      if (_menuIdx == 0) _startPlay();
      else if (_menuIdx == 1) _diff = (_diff + 1) % kDiffN;
      else if (_menuIdx == 2) _showScores();
      else Screen.goBack();
    }
  } else if (_state == ST_PLAY) {
    int step = (dir == INavigation::DIR_LEFT || dir == INavigation::DIR_RIGHT) ? 10 : 1;
    if (dir == INavigation::DIR_UP || dir == INavigation::DIR_RIGHT)   _guess = min(_guess + step, kMaxVal[_diff]);
    if (dir == INavigation::DIR_DOWN || dir == INavigation::DIR_LEFT)   _guess = max(_guess - step, 0);
    if (dir == INavigation::DIR_PRESS) { _submit(); return; }
    if (dir == INavigation::DIR_BACK) { _showMenu(); return; }
    _paintValue();
  } else if (_state == ST_SCORES) {
    if (dir != INavigation::DIR_NONE) _showMenu();
  } else if (_state == ST_RESULT) {
    if (dir == INavigation::DIR_PRESS) _startPlay();
    else if (dir == INavigation::DIR_BACK) _showMenu();
  }
}

void GameNumberGuessScreen::onRender()
{
  LvglUi::pump();
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────
#include "core/ConfigManager.h"
#include "screens/game/GameMenuScreen.h"

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#include "legacy_game_numberguess.inc"

#endif
