//
// Number Guess — native LVGL build (USE_LVGL_SHELL). Big value cell, hint
// arrows, a shrinking knowledge-range bar, GameNav held-repeat stepping.
//

#include "GameNumberGuessScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)

const char* const GameNumberGuessScreen::kDiffNames[kDiffN] = {
  "EASY 0-99", "MEDIUM 0-999", "HARD 0-9999", "EXTREME 0-9999"
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
  _arena = gamekit::arena(parent);

  gamekit::label(_arena, &lv_font_montserrat_16, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 4, "GUESS THE NUMBER");
  _triesLbl = gamekit::label(_arena, &lv_font_montserrat_14, 0xcfd8e3,
                              LV_ALIGN_TOP_RIGHT, -8, 28, "");

  _valueBox = gamekit::cell(_arena, 80, 62, 160, 48, 0x3e5080, 10);
  lv_obj_set_style_text_font(_valueBox, &lv_font_montserrat_20, 0);
  gamekit::setCellText(_valueBox, ' ');

  _hint = gamekit::label(_arena, &lv_font_montserrat_20, 0xcfd8e3,
                         LV_ALIGN_TOP_MID, 0, 122, "");

  // knowledge-range bar: track + highlighted known window
  _rangeTrack = lv_obj_create(_arena);
  lv_obj_set_pos(_rangeTrack, 20, 168);
  lv_obj_set_size(_rangeTrack, 280, 10);
  lv_obj_set_style_bg_color(_rangeTrack, lv_color_hex(0x24345c), 0);
  lv_obj_set_style_bg_opa(_rangeTrack, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(_rangeTrack, lv_color_hex(0x0d1321), 0);
  lv_obj_set_style_border_width(_rangeTrack, 1, 0);
  lv_obj_set_style_radius(_rangeTrack, 5, 0);

  _rangeBar = lv_obj_create(_arena);
  lv_obj_set_pos(_rangeBar, 20, 166);
  lv_obj_set_size(_rangeBar, 280, 14);
  lv_obj_set_style_bg_color(_rangeBar, lv_color_hex(0x43a047), 0);
  lv_obj_set_style_bg_opa(_rangeBar, LV_OPA_60, 0);
  lv_obj_set_style_border_width(_rangeBar, 0, 0);
  lv_obj_set_style_radius(_rangeBar, 6, 0);

  gamekit::label(_arena, &lv_font_montserrat_14, 0x8fa0b8,
                 LV_ALIGN_BOTTOM_MID, 0, -2,
                 "UP/DOWN = +-1   LEFT/RIGHT = +-10   PRESS = try");
}

void GameNumberGuessScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameNumberGuessScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _menuIdx = 0;

  _overlay = gamekit::panel(_arena, 250, 186);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 8, "NUMBER GUESS");
  const char* rows[4] = { "Play", "Difficulty", "Best", "Exit" };
  for (int i = 0; i < 4; i++)
    _ovRows[i] = gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                                LV_ALIGN_TOP_MID, 0, 42 + i * 32, rows[i]);

  lv_label_set_text(_triesLbl, "");
  lv_label_set_text(_hint, "");
  gamekit::setCellText(_valueBox, ' ');
  gamekit::paintCell(_valueBox, 0x3e5080);
}

void GameNumberGuessScreen::_showScores()
{
  _state = ST_SCORES;
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 250, 170);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 8, "BEST (fewest tries)");
  char buf[44];
  for (int i = 0; i < kDiffN; i++) {
    bool played = _best[i] < 900;
    snprintf(buf, sizeof(buf), "%-12s %s", kDiffNames[i],
             played ? String(_best[i]).c_str() : "-");
    gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                   LV_ALIGN_TOP_LEFT, 20, 48 + i * 28, buf);
  }
}

void GameNumberGuessScreen::_showResult(bool win)
{
  _state = ST_RESULT;
  _hideOverlay();

  if (win && (_best[_diff] > _tries || _best[_diff] >= 900)) {
    _best[_diff] = _tries;
    Config.set(String("ng_best_") + _diff, String(_tries));
  }

  _overlay = gamekit::panel(_arena, 250, 160, win ? 0x24422e : 0x3a2a35);
  lv_obj_set_style_border_color(_overlay,
    lv_color_hex(win ? 0x43a047 : 0xff7043), 0);
  gamekit::label(_overlay, &lv_font_montserrat_20,
                 win ? 0x62d49c : 0xff7043, LV_ALIGN_TOP_MID, 0, 12,
                 win ? "CORRECT!" : "OUT OF TRIES");
  char buf[56];
  snprintf(buf, sizeof(buf), "the number was %d   tries %d", _target, _tries);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 54, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 104, "PRESS = retry    BACK = menu");
}

void GameNumberGuessScreen::_startPlay()
{
  _hideOverlay();
  _target = random(0, kMaxVal[_diff] + 1);
  _guess  = kMaxVal[_diff] / 2;
  _tries  = 0;
  _lo = 0; _hi = kMaxVal[_diff];
  _state  = ST_PLAY;
  lv_label_set_text(_triesLbl, "");
  lv_label_set_text(_hint, "make a guess...");
  _paintValue();
  _paintRange();
}

void GameNumberGuessScreen::_paintValue()
{
  lv_label_set_text(_valueBox, String(_guess).c_str());
}

void GameNumberGuessScreen::_paintRange()
{
  int span = kMaxVal[_diff];
  int x = 20 + (int)((long)_lo * 280 / span);
  int w = (int)((long)(_hi - _lo) * 280 / span);
  if (w < 4) w = 4;
  lv_obj_set_pos(_rangeBar, x, 166);
  lv_obj_set_size(_rangeBar, w, 14);
}

void GameNumberGuessScreen::_submit()
{
  _tries++;
  if (kTrials[_diff] > 0)
    lv_label_set_text(_triesLbl, (String("tries ") + _tries + " / " + kTrials[_diff]).c_str());
  else
    lv_label_set_text(_triesLbl, (String("tries ") + _tries).c_str());

  if (_guess == _target) {
    if (Uni.Speaker) Uni.Speaker->playWin();
    int n = Achievement.inc("numguess_first_win");
    if (n == 1) Achievement.unlock("numguess_first_win");
    _showResult(true);
    return;
  }
  if (_guess < _target) {
    _lo = max(_lo, _guess + 1);
    lv_label_set_text(_hint, LV_SYMBOL_UP "  higher");
    lv_obj_set_style_text_color(_hint, lv_color_hex(0x62d49c), 0);
  } else {
    _hi = min(_hi, _guess - 1);
    lv_label_set_text(_hint, LV_SYMBOL_DOWN "  lower");
    lv_obj_set_style_text_color(_hint, lv_color_hex(0xffb380), 0);
  }
  _paintRange();
  if (kTrials[_diff] > 0 && _tries >= kTrials[_diff]) {
    if (Uni.Speaker) Uni.Speaker->playLose();
    _showResult(false);
  }
}

void GameNumberGuessScreen::_menuFocus()
{
  if (_state != ST_MENU || !_overlay) return;
  for (int i = 0; i < 4; i++) gamekit::focusRow(_ovRows[i], i == _menuIdx);
  char db[44];
  snprintf(db, sizeof(db), "Difficulty: %s", kDiffNames[_diff]);
  lv_label_set_text(_ovRows[1], db);
}

void GameNumberGuessScreen::onUpdate()
{
  _menuFocus();

  auto dir = _nav.poll();
  if (dir == INavigation::DIR_NONE) return;

  if (_state == ST_MENU) {
    if      (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + 3) % 4;
    else if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % 4;
    else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    else if (dir == INavigation::DIR_PRESS) {
      if      (_menuIdx == 0) _startPlay();
      else if (_menuIdx == 1) _diff = (_diff + 1) % kDiffN;
      else if (_menuIdx == 2) _showScores();
      else                   { Screen.goBack(); return; }
    }
  } else if (_state == ST_PLAY) {
    int step = (dir == INavigation::DIR_LEFT || dir == INavigation::DIR_RIGHT) ? 10 : 1;
    if      (dir == INavigation::DIR_UP || dir == INavigation::DIR_RIGHT)
      _guess = min(_guess + step, kMaxVal[_diff]);
    else if (dir == INavigation::DIR_DOWN || dir == INavigation::DIR_LEFT)
      _guess = max(_guess - step, 0);
    else if (dir == INavigation::DIR_PRESS) { _submit(); return; }
    else if (dir == INavigation::DIR_BACK)  { _showMenu(); return; }
    _paintValue();
  } else if (_state == ST_SCORES) {
    _showMenu();
  } else if (_state == ST_RESULT) {
    if      (dir == INavigation::DIR_PRESS) _startPlay();
    else if (dir == INavigation::DIR_BACK)  _showMenu();
  }
}

void GameNumberGuessScreen::onRender()
{
  // Live updates happen in onUpdate().
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────
#include "core/ConfigManager.h"
#include "screens/game/GameMenuScreen.h"

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#include "legacy_game_numberguess.inc"

#endif
