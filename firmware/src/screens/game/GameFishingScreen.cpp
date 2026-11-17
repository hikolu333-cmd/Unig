//
// Fishing — native LVGL build (USE_LVGL_SHELL). Timing bar as lv_obj pieces:
// track, green zone, sweeping marker. Cast → wait → bite → hook in the green
// → reel (hit the green N times). Fish rarity from the legacy odds.
//

#include "GameFishingScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#ifdef USE_LVGL_SHELL

namespace {
const char* const kFishNames[3] = { "COMMON", "RARE", "LEGENDARY" };
const uint32_t kFishColors[3]   = { 0x4a7ba6, 0xd4a017, 0xff7043 };   // RGB888 → lv_color_hex
const int      kFishScores[3]   = { 10, 40, 120 };
const uint8_t  kFishHits[3]     = { 2, 3, 4 };
const uint16_t kFishGreen[3]    = { 64, 44, 26 };
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

  _arena = lv_obj_create(parent);
  lv_obj_set_pos(_arena, 0, 0);
  lv_obj_set_size(_arena, lv_obj_get_width(parent), lv_obj_get_height(parent));
  lv_obj_set_style_bg_color(_arena, lv_color_hex(0x0b1522), 0);
  lv_obj_set_style_bg_opa(_arena, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_arena, 0, 0);
  lv_obj_set_style_pad_all(_arena, 0, 0);
  lv_obj_set_style_radius(_arena, 0, 0);
  lv_obj_clear_flag(_arena, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* t = lv_label_create(_arena);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 4);
  lv_label_set_text(t, "FISHING");

  _scoreLbl = lv_label_create(_arena);
  lv_obj_set_style_text_font(_scoreLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_scoreLbl, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(_scoreLbl, LV_ALIGN_TOP_RIGHT, -8, 30);
  lv_label_set_text(_scoreLbl, "");

  _statusLbl = lv_label_create(_arena);
  lv_obj_set_style_text_font(_statusLbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(_statusLbl, lv_color_white(), 0);
  lv_obj_align(_statusLbl, LV_ALIGN_TOP_MID, 0, 58);
  lv_label_set_text(_statusLbl, "");

  // timing bar
  _barW = 240;
  _barX = (lv_obj_get_width(_arena) - _barW) / 2;
  _barY = lv_obj_get_height(_arena) / 2 + 4;

  _barBg = lv_obj_create(_arena);
  lv_obj_set_pos(_barBg, _barX, _barY);
  lv_obj_set_size(_barBg, _barW, 18);
  lv_obj_set_style_bg_color(_barBg, lv_color_hex(0x182338), 0);
  lv_obj_set_style_bg_opa(_barBg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_barBg, 0, 0);
  lv_obj_set_style_radius(_barBg, 4, 0);
  lv_obj_add_flag(_barBg, LV_OBJ_FLAG_HIDDEN);

  _barGreen = lv_obj_create(_arena);
  lv_obj_set_pos(_barGreen, _barX, _barY);
  lv_obj_set_size(_barGreen, _greenW, 18);
  lv_obj_set_style_bg_color(_barGreen, lv_color_hex(0x2e8b57), 0);
  lv_obj_set_style_bg_opa(_barGreen, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_barGreen, 0, 0);
  lv_obj_set_style_radius(_barGreen, 4, 0);
  lv_obj_add_flag(_barGreen, LV_OBJ_FLAG_HIDDEN);

  _marker = lv_obj_create(_arena);
  lv_obj_set_size(_marker, 6, 26);
  lv_obj_set_style_bg_color(_marker, lv_color_hex(0xffd54f), 0);
  lv_obj_set_style_bg_opa(_marker, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_marker, 0, 0);
  lv_obj_set_style_radius(_marker, 3, 0);
  lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
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

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 220, 140);
  lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x101a26), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  const char* rows[kMenuN] = { "Cast", "Best", "Exit" };
  for (int i = 0; i < kMenuN; i++) {
    _ovRows[i] = lv_label_create(_overlay);
    lv_obj_set_style_text_font(_ovRows[i], &lv_font_montserrat_16, 0);
    lv_obj_align(_ovRows[i], LV_ALIGN_TOP_MID, 0, i * 38);
    lv_label_set_text(_ovRows[i], rows[i]);
  }
  lv_obj_add_flag(_barBg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_barGreen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_statusLbl, "");
  lv_label_set_text(_scoreLbl, "");
}

void GameFishingScreen::_showResult()
{
  _state = ST_RESULT;
  _hideOverlay();
  lv_obj_add_flag(_barBg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_barGreen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_statusLbl, "");

  if (_sessionScore > _best) {
    _best = _sessionScore;
    Config.set("fish_best", String(_best));
    int n = Achievement.inc("fishing_best");
    if (n == 1) Achievement.unlock("fishing_best");
  }

  _overlay = lv_obj_create(_arena);
  lv_obj_center(_overlay);
  lv_obj_set_size(_overlay, 240, 140);
  lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x0d1a12), 0);
  lv_obj_set_style_bg_opa(_overlay, LV_OPA_90, 0);
  lv_obj_set_style_border_color(_overlay, lv_color_hex(0x2e8b57), 0);
  lv_obj_set_style_border_width(_overlay, 2, 0);
  lv_obj_set_style_radius(_overlay, 10, 0);
  lv_obj_set_style_pad_all(_overlay, 10, 0);
  lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* t = lv_label_create(_overlay);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0x62d49c), 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 8);
  lv_label_set_text(t, "DAY'S CATCH");

  char buf[56];
  snprintf(buf, sizeof(buf), "score %d   fish %d   best %d",
           _sessionScore, _catchCount, _best);
  lv_obj_t* s = lv_label_create(_overlay);
  lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s, lv_color_white(), 0);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 42);
  lv_label_set_text(s, buf);

  lv_obj_t* h = lv_label_create(_overlay);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(0x9aa5b1), 0);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 70);
  lv_label_set_text(h, "PRESS = again   BACK = menu");
}

void GameFishingScreen::_startCast()
{
  _hideOverlay();
  _state = ST_CAST;
  _stateT0 = millis();

  int r = random(0, 100);
  if      (r < 10) { _fishType = FISH_LEGENDARY; _biteAtMs = _stateT0 + random(1500, 3000); }
  else if (r < 40) { _fishType = FISH_RARE;      _biteAtMs = _stateT0 + random(2500, 5000); }
  else             { _fishType = FISH_COMMON;    _biteAtMs = _stateT0 + random(3500, 7000); }
  _biteUntilMs = _biteAtMs + 900;

  _fishScore  = kFishScores[_fishType];
  _hitsNeeded = kFishHits[_fishType];
  _greenW     = kFishGreen[_fishType];
  _greenX     = (uint16_t)random(0, _barW - _greenW);

  lv_obj_add_flag(_barBg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_barGreen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_statusLbl, "casting… wait for the bite");
  char sb[32];
  snprintf(sb, sizeof(sb), "score %d   fish %d", _sessionScore, _catchCount);
  lv_label_set_text(_scoreLbl, sb);
}

void GameFishingScreen::_openBite()
{
  _state = ST_BITE;
  _markerPos = 0;
  _markerDir = 1;
  _markerLastMs = millis();

  lv_obj_clear_flag(_barBg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(_barGreen, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_statusLbl, "BITE!  PRESS inside the green");
  _syncBar();
}

void GameFishingScreen::_tickMarker()
{
  uint32_t now = millis();
  float dt = (now - _markerLastMs) / 1000.0f;
  _markerLastMs = now;
  _markerPos += _markerDir * 1.6f * dt;
  if (_markerPos >= 1.0f) { _markerPos = 1.0f; _markerDir = -1; }
  if (_markerPos <= 0.0f) { _markerPos = 0.0f; _markerDir = 1; }
}

void GameFishingScreen::_syncBar()
{
  lv_obj_set_pos(_barGreen, _barX + _greenX, _barY);
  lv_obj_set_size(_barGreen, _greenW, 18);
  lv_obj_set_x(_marker, _barX + (int)(_markerPos * (_barW - 6)));
  lv_obj_set_y(_marker, _barY - 4);
}

void GameFishingScreen::_hookCheck()
{
  int mx = (int)(_markerPos * (_barW - 6)) + 3;
  int gx = _greenX;
  bool hit = (mx >= gx && mx <= gx + (int)_greenW);
  if (hit) {
    _hits = 0;
    _state = ST_REEL;
    _greenX = (uint16_t)random(0, _barW - _greenW);
    char sb[40];
    snprintf(sb, sizeof(sb), "%s on the line! hit the green", kFishNames[_fishType]);
    lv_label_set_text(_statusLbl, sb);
    lv_obj_set_style_bg_color(_marker, lv_color_hex(kFishColors[_fishType]), 0);
    _syncBar();
  } else {
    _state = ST_ESCAPED;
    _flashUntil = millis() + 900;
    lv_label_set_text(_statusLbl, "it got away…");
    lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  }
}

void GameFishingScreen::_syncWidgets()
{
  if (_state == ST_BITE) {
    _tickMarker();
    _syncBar();
  } else if (_state == ST_REEL) {
    _tickMarker();
    _syncBar();
    char sb[48];
    snprintf(sb, sizeof(sb), "reel %d / %d", _hits, _hitsNeeded);
    lv_obj_t* lbl = _statusLbl;
    lv_label_set_text(lbl, sb);
  }
}

void GameFishingScreen::onUpdate()
{
  // menu focus
  if (_state == ST_MENU && _overlay) {
    for (int i = 0; i < kMenuN; i++) {
      bool sel = (i == _menuIdx);
      lv_obj_set_style_text_color(_ovRows[i], sel ? lv_color_black() : lv_color_white(), 0);
      lv_obj_set_style_bg_color(_ovRows[i],
        sel ? LvglUi::c565(Config.getThemeColor()) : lv_color_hex(0x101a26), 0);
      lv_obj_set_style_bg_opa(_ovRows[i], LV_OPA_COVER, 0);
      lv_obj_set_style_pad_hor(_ovRows[i], 16, 0);
      lv_obj_set_style_pad_ver(_ovRows[i], 5, 0);
      lv_obj_set_style_radius(_ovRows[i], 5, 0);
    }
  }

  uint32_t now = millis();

  if (_state == ST_CAST && now >= _biteAtMs) _openBite();
  if (_state == ST_BITE && now >= _biteUntilMs) {
    _state = ST_ESCAPED;
    _flashUntil = now + 900;
    lv_label_set_text(_statusLbl, "too slow — it got away");
    lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
  }
  if (_state == ST_ESCAPED && now >= _flashUntil) _startCast();

  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();

    if (_state == ST_MENU) {
      if (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + kMenuN - 1) % kMenuN;
      if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % kMenuN;
      if (dir == INavigation::DIR_PRESS) {
        if (_menuIdx == 0) { _sessionScore = 0; _catchCount = 0; _startCast(); }
        else if (_menuIdx == 1) _showResult();
        else Screen.goBack();
      }
    } else if (_state == ST_CAST || _state == ST_ESCAPED || _state == ST_CAUGHT || _state == ST_BITE) {
      if (dir == INavigation::DIR_PRESS && _state == ST_BITE) _hookCheck();
      else if (dir == INavigation::DIR_BACK) { _showResult(); }
    } else if (_state == ST_REEL) {
      if (dir == INavigation::DIR_PRESS) {
        int mx = (int)(_markerPos * (_barW - 6)) + 3;
        bool hit = (mx >= (int)_greenX && mx <= (int)_greenX + (int)_greenW);
        if (hit) {
          _hits++;
          if (Uni.Speaker) Uni.Speaker->playRandomTone(0, 40);
          if (_hits >= _hitsNeeded) {
            _catchCount++;
            _sessionScore += _fishScore;
            _state = ST_CAUGHT;
            _flashUntil = now + 1100;
            char sb[48];
            snprintf(sb, sizeof(sb), "caught %s! +%d", kFishNames[_fishType], _fishScore);
            lv_label_set_text(_statusLbl, sb);
            lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
            int n = Achievement.inc("fishing_catch");
            if (n == 1) Achievement.unlock("fishing_catch");
          } else {
            _greenX = (uint16_t)random(0, _barW - _greenW);
          }
        } else {
          _state = ST_ESCAPED;
          _flashUntil = now + 900;
          lv_label_set_text(_statusLbl, "line snapped…");
          lv_obj_add_flag(_marker, LV_OBJ_FLAG_HIDDEN);
        }
      } else if (dir == INavigation::DIR_BACK) { _showResult(); }
    } else if (_state == ST_CAUGHT && now >= _flashUntil) {
      _startCast();
    } else if (_state == ST_RESULT) {
      if (dir == INavigation::DIR_PRESS) { _sessionScore = 0; _catchCount = 0; _startCast(); }
      else if (dir == INavigation::DIR_BACK) _showMenu();
    }
  }
}

void GameFishingScreen::onRender()
{
  _syncWidgets();
  LvglUi::pump();
}

#else  // ── legacy TFT (original from the repo) ───────────────────────────────

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "core/ConfigManager.h"

#include "legacy_game_fishing.inc"

#endif
