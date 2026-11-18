//
// Fishing — native LVGL build (USE_LVGL_SHELL). Bright water, timing bar
// with a green catch zone, sweeping marker. Rarer fish = faster bite,
// more reel hits, narrower zone.
//

#include "GameFishingScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)

namespace {
const char* const kFishNames[3]  = { "COMMON", "RARE", "LEGENDARY" };
const uint32_t kFishColors[3]    = { 0x9ec9e8, 0xffd54f, 0xff7043 };
const int      kFishScores[3]    = { 10, 40, 120 };
const uint8_t  kFishHits[3]      = { 2, 3, 4 };
const int      kFishGreen[3]     = { 70, 50, 30 };
const float    kFishSpeed[3]     = { 1.1f, 1.4f, 1.8f };
}

void GameFishingScreen::onInit()
{
  _best = Config.get("fish_best", "0").toInt();
  _build();
  _showMenu();
}

void GameFishingScreen::_build()
{
  lv_obj_t* parent = LvglUi::viewParent();
  _arena = gamekit::arena(parent, 0x2c6d9e);      // bright water

  // decorative wave stripes
  for (int i = 0; i < 2; i++) {
    lv_obj_t* w = lv_obj_create(_arena);
    lv_obj_set_pos(w, -20 + i * 30, 48 + i * 20);
    lv_obj_set_size(w, 330, 4);
    lv_obj_set_style_bg_color(w, lv_color_hex(0x1d3a54), 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_60, 0);
    lv_obj_set_style_border_width(w, 0, 0);
    lv_obj_set_style_radius(w, 2, 0);
  }

  gamekit::label(_arena, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 4, "FISHING");
  _scoreLbl = gamekit::label(_arena, &lv_font_montserrat_14, 0xffffff,
                              LV_ALIGN_TOP_RIGHT, -8, 30, "");
  _statusLbl = gamekit::label(_arena, &lv_font_montserrat_16, 0xffffff,
                              LV_ALIGN_TOP_MID, 0, 62, "");

  // timing bar: bright track + green zone + red marker
  _track = lv_obj_create(_arena);
  lv_obj_set_pos(_track, kBarX, kBarY);
  lv_obj_set_size(_track, kBarW, 18);
  lv_obj_set_style_bg_color(_track, lv_color_hex(0x24304c), 0);
  lv_obj_set_style_bg_opa(_track, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(_track, lv_color_hex(0x0d1321), 0);
  lv_obj_set_style_border_width(_track, 2, 0);
  lv_obj_set_style_radius(_track, 9, 0);

  _zone = lv_obj_create(_arena);
  lv_obj_set_pos(_zone, kBarX, kBarY);
  lv_obj_set_size(_zone, 60, 18);
  lv_obj_set_style_bg_color(_zone, lv_color_hex(0x43a047), 0);
  lv_obj_set_style_bg_opa(_zone, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_zone, 0, 0);
  lv_obj_set_style_radius(_zone, 8, 0);

  _marker = lv_obj_create(_arena);
  lv_obj_set_size(_marker, 6, 26);
  lv_obj_set_style_bg_color(_marker, lv_color_hex(0xd32f2f), 0);
  lv_obj_set_style_bg_opa(_marker, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_marker, 0, 0);
  lv_obj_set_style_radius(_marker, 3, 0);

  _barVisible(false);
}

void GameFishingScreen::_barVisible(bool v)
{
  if (v) {
    lv_obj_clear_flag(_track, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_zone, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(_track, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_zone, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  }
}

void GameFishingScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameFishingScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _menuIdx = 0;

  _overlay = gamekit::panel(_arena, 220, 160, 0x24345c);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 10, "FISHING");
  const char* rows[kMenuN] = { "Cast", "Day's Catch", "Exit" };
  for (int i = 0; i < kMenuN; i++)
    _ovRows[i] = gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                                LV_ALIGN_TOP_MID, 0, 46 + i * 32, rows[i]);

  _barVisible(false);
  lv_label_set_text(_statusLbl, "");
  lv_label_set_text(_scoreLbl, "");
}

void GameFishingScreen::_showResult()
{
  _state = ST_RESULT;
  _hideOverlay();
  _barVisible(false);
  lv_label_set_text(_statusLbl, "");

  if (_sessionScore > _best) {
    _best = _sessionScore;
    Config.set("fish_best", String(_best));
    int n = Achievement.inc("fishing_best");
    if (n == 1) Achievement.unlock("fishing_best");
  }

  _overlay = gamekit::panel(_arena, 260, 170, 0x24422e);
  lv_obj_set_style_border_color(_overlay, lv_color_hex(0x43a047), 0);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0x62d49c,
                 LV_ALIGN_TOP_MID, 0, 12, "DAY'S CATCH");
  char buf[56];
  snprintf(buf, sizeof(buf), "score %d    fish %d", _sessionScore, _catchCount);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 52, buf);
  snprintf(buf, sizeof(buf), "best %d", _best);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 80, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 116, "PRESS = again    BACK = menu");
}

void GameFishingScreen::_startCast()
{
  _hideOverlay();
  _state = ST_CAST;

  uint32_t now = millis();
  int r = random(0, 100);
  if      (r < 10) { _fishType = FISH_LEGENDARY; _biteAtMs = now + random(1500, 3000); }
  else if (r < 40) { _fishType = FISH_RARE;      _biteAtMs = now + random(2500, 5000); }
  else             { _fishType = FISH_COMMON;    _biteAtMs = now + random(3500, 7000); }
  _biteUntilMs = _biteAtMs + 900;

  _fishScore  = kFishScores[_fishType];
  _hitsNeeded = kFishHits[_fishType];
  _hits       = 0;
  _greenW     = kFishGreen[_fishType];
  _greenX     = random(0, kBarW - _greenW);

  _barVisible(false);
  lv_label_set_text(_statusLbl, "casting... wait for the bite");
  char sb[40];
  snprintf(sb, sizeof(sb), "score %d   fish %d", _sessionScore, _catchCount);
  lv_label_set_text(_scoreLbl, sb);
}

void GameFishingScreen::_openBite()
{
  _state = ST_BITE;
  _markerPos = 0;
  _markerDir = 1;
  _markerLastMs = millis();

  _barVisible(true);
  _syncBar();
  lv_label_set_text(_statusLbl, "BITE!  PRESS inside the green");
  if (Uni.Speaker) Uni.Speaker->playRandomTone(0, 60);
}

void GameFishingScreen::_tickMarker()
{
  uint32_t now = millis();
  float dt = (now - _markerLastMs) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;
  _markerLastMs = now;
  _markerPos += _markerDir * kFishSpeed[_fishType] * dt;
  if (_markerPos >= 1.0f) { _markerPos = 1.0f; _markerDir = -1; }
  if (_markerPos <= 0.0f) { _markerPos = 0.0f; _markerDir = 1; }
}

void GameFishingScreen::_syncBar()
{
  lv_obj_set_pos(_zone, kBarX + _greenX, kBarY);
  lv_obj_set_size(_zone, _greenW, 18);
  int mx = kBarX + (int)(_markerPos * (kBarW - 6));
  lv_obj_set_pos(_marker, mx, kBarY - 4);
}

void GameFishingScreen::_hookCheck()
{
  int mx = (int)(_markerPos * (kBarW - 6)) + 3;
  bool hit = (mx >= _greenX && mx <= _greenX + _greenW);
  if (hit) {
    _hits = 0;
    _state = ST_REEL;
    _greenX = random(0, kBarW - _greenW);
    char sb[48];
    snprintf(sb, sizeof(sb), "%s on the line! hit the green x%u",
             kFishNames[_fishType], _hitsNeeded);
    lv_label_set_text(_statusLbl, sb);
    if (Uni.Speaker) Uni.Speaker->playRandomTone(0, 80);
    _syncBar();
  } else {
    _state = ST_ESCAPED;
    _flashUntil = millis() + 900;
    lv_label_set_text(_statusLbl, "it got away...");
    _barVisible(false);
    if (Uni.Speaker) Uni.Speaker->playLose();
  }
}

void GameFishingScreen::_menuFocus()
{
  if (_state != ST_MENU || !_overlay) return;
  for (int i = 0; i < kMenuN; i++) gamekit::focusRow(_ovRows[i], i == _menuIdx);
}

void GameFishingScreen::onUpdate()
{
  uint32_t now = millis();
  _menuFocus();

  if (_state == ST_CAST && now >= _biteAtMs) _openBite();
  if (_state == ST_BITE && now >= _biteUntilMs) {
    _state = ST_ESCAPED;
    _flashUntil = now + 900;
    lv_label_set_text(_statusLbl, "too slow - it got away");
    _barVisible(false);
  }
  if (_state == ST_ESCAPED && now >= _flashUntil) _startCast();
  if (_state == ST_CAUGHT && now >= _flashUntil) _startCast();

  if (_state == ST_BITE || _state == ST_REEL) {
    _tickMarker();
    _syncBar();
  }
  if (_state == ST_REEL) {
    char sb[48];
    snprintf(sb, sizeof(sb), "reel %u / %u", _hits, _hitsNeeded);
    lv_label_set_text(_statusLbl, sb);
  }

  auto dir = _nav.poll();
  if (dir == INavigation::DIR_NONE) return;

  if (_state == ST_MENU) {
    if      (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + kMenuN - 1) % kMenuN;
    else if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % kMenuN;
    else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
    else if (dir == INavigation::DIR_PRESS) {
      if      (_menuIdx == 0) { _sessionScore = 0; _catchCount = 0; _startCast(); }
      else if (_menuIdx == 1) _showResult();
      else                   { Screen.goBack(); return; }
    }
  } else if (_state == ST_BITE) {
    if (dir == INavigation::DIR_PRESS) _hookCheck();
    else if (dir == INavigation::DIR_BACK) _showResult();
  } else if (_state == ST_REEL) {
    if (dir == INavigation::DIR_PRESS) {
      int mx = (int)(_markerPos * (kBarW - 6)) + 3;
      bool hit = (mx >= _greenX && mx <= _greenX + _greenW);
      if (hit) {
        _hits++;
        if (Uni.Speaker) Uni.Speaker->playRandomTone(0, 40);
        if (_hits >= _hitsNeeded) {
          _catchCount++;
          _sessionScore += _fishScore;
          _state = ST_CAUGHT;
          _flashUntil = now + 1100;
          char sb[48];
          snprintf(sb, sizeof(sb), "caught %s!  +%d", kFishNames[_fishType], _fishScore);
          lv_label_set_text(_statusLbl, sb);
          _barVisible(false);
          int n = Achievement.inc("fishing_catch");
          if (n == 1) Achievement.unlock("fishing_catch");
        } else {
          _greenX = random(0, kBarW - _greenW);
        }
      } else {
        _state = ST_ESCAPED;
        _flashUntil = now + 900;
        lv_label_set_text(_statusLbl, "line snapped...");
        _barVisible(false);
        if (Uni.Speaker) Uni.Speaker->playLose();
      }
    } else if (dir == INavigation::DIR_BACK) {
      _showResult();
    }
  } else if (_state == ST_CAST || _state == ST_CAUGHT) {
    if (dir == INavigation::DIR_BACK) _showResult();
  } else if (_state == ST_RESULT) {
    if      (dir == INavigation::DIR_PRESS) { _sessionScore = 0; _catchCount = 0; _startCast(); }
    else if (dir == INavigation::DIR_BACK)   _showMenu();
  }
}

void GameFishingScreen::onRender()
{
  // Live updates happen in onUpdate().
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#include "legacy_game_fishing.inc"

#endif
