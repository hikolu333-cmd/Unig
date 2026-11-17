#pragma once

#include "ui/templates/BaseScreen.h"

#ifdef USE_LVGL_SHELL
// ─────────────────────────────────────────────────────────────────────────────
// Fishing — native LVGL build. Cast → random bite window → timing bar: a
// marker sweeps, PRESS inside the green zone hooks the fish; rarer fish have
// narrower zones and bite sooner. Session score, best in Config.
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"

class GameFishingScreen : public BaseScreen
{
public:
  const char* title()     override { return "Fishing"; }
  bool inhibitPowerSave() override { return true; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_MENU, ST_CAST, ST_BITE, ST_REEL, ST_CAUGHT, ST_ESCAPED, ST_RESULT };
  enum FishType { FISH_COMMON = 0, FISH_RARE, FISH_LEGENDARY };

  static constexpr uint8_t kMenuN = 3;

  State    _state = ST_MENU;
  int8_t   _menuIdx = 0;
  int      _sessionScore = 0;
  int      _catchCount   = 0;
  int      _best = 0;
  FishType _fishType = FISH_COMMON;
  int      _fishScore = 0;
  uint32_t _biteAtMs = 0;      // when the bite window opens
  uint32_t _biteUntilMs = 0;
  uint32_t _stateT0 = 0;
  // timing bar
  int16_t  _barW = 240, _barX = 0, _barY = 0;
  uint16_t _greenW = 60, _greenX = 0;
  float    _markerPos = 0;
  int8_t   _markerDir = 1;
  uint32_t _markerLastMs = 0;
  // reel minigame
  uint8_t  _hits = 0, _hitsNeeded = 0;
  uint32_t _flashUntil = 0;

  lv_obj_t* _arena = nullptr;
  lv_obj_t* _statusLbl = nullptr;
  lv_obj_t* _scoreLbl = nullptr;
  lv_obj_t* _barBg = nullptr;
  lv_obj_t* _barGreen = nullptr;
  lv_obj_t* _marker = nullptr;
  lv_obj_t* _overlay = nullptr;
  lv_obj_t* _ovRows[kMenuN] = {};

  void _build();
  void _showMenu();
  void _hideOverlay();
  void _showResult();
  void _startCast();
  void _openBite();
  void _hookCheck();
  void _tickMarker();
  void _syncBar();
  void _syncWidgets();
};

#else  // legacy — original from the repository

// ── Legacy: original header from the repository (verbatim) ──

#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/SpriteCanvas.h"
#endif

class GameFishingScreen : public BaseScreen
{
public:
  const char* title()       override { return "Fishing"; }
  bool inhibitPowerSave()   override { return _state == STATE_CAST || _state == STATE_BITE || _state == STATE_REEL; }
  bool inhibitPowerOff()    override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State {
    STATE_MENU,
    STATE_CAST,
    STATE_BITE,
    STATE_REEL,
    STATE_CAUGHT,
    STATE_ESCAPED,
    STATE_RESULT,
    STATE_HIGH_SCORES
  } _state = STATE_MENU;

  enum FishType { FISH_COMMON = 0, FISH_RARE, FISH_LEGENDARY };

  static constexpr uint8_t  kMenuItems  = 3;
  static constexpr uint8_t  kTopN       = 5;
  static constexpr uint8_t  kIndicatorW = 4;
  static constexpr const char* kHsFile  = "/unigeek/games/fishing_hs.txt";

  // State tracking
  State   _prevState   = (State)0xFF;

  // Menu
  int8_t  _menuIdx     = 0;
  int8_t  _lastMenuIdx = -1;

  // Session
  int   _sessionScore = 0;
  int   _catchCount   = 0;
  int   _lastScore    = 0;
  int   _bestScore    = 0;
  bool  _isNewHigh    = false;

  // Fish type for current catch
  FishType _fishType    = FISH_COMMON;
  int      _fishScore   = 0;
  uint8_t  _hitsNeeded  = 0;
  float    _tensionRate = 0;

  // Frame/state timing
  uint32_t _lastFrameMs = 0;
  uint32_t _stateTimer  = 0;
  uint32_t _biteDelay   = 0;

  // Cast/bite animation
  float    _bobberPhase = 0;
  float    _bobberY     = 0;
  float    _wavePhase   = 0;
  uint8_t  _rippleR     = 0;
  uint32_t _rippleTimer = 0;

  // Reel minigame
  float    _reelPos       = 0;
  float    _reelDir       = 1.0f;
  float    _reelSpeed     = 0;
  float    _tension       = 0;
  uint16_t _greenX        = 0;
  uint8_t  _greenW        = 0;
  uint8_t  _catchProgress = 0;
  bool     _perfectReel   = true;

  // Partial-redraw tracking (reel)
  int16_t  _prevReelPos  = -1;
  int16_t  _prevGreenX   = -1;
  uint8_t  _prevCatchPct = 0xFF;
  uint8_t  _prevTension  = 0xFF;

  // High scores
  int _topScores[kTopN] = {};

  void _update();
  void _initCast();
  void _initBite();
  void _initReel();
  void _selectFishType();
  void _pressInReel();
  void _checkAchievements();

  void _renderMenu();
  void _renderCast();
  void _renderBite();
  void _renderReel();
  void _renderCaught();
  void _renderEscaped();
  void _renderResult();
  void _renderHighScores();

  void _loadHighScores();
  void _saveHighScore();
  void _drawWave(int bx, int waterTop, int bw);
  void _drawSceneChrome(int bx, int by, int bw, int bh);

#ifdef USE_LVGL_SHELL
  // Full-body pixel game — bridged via canvas; origin shifts to (0,0).
  SpriteCanvas _scene;
  Sprite*      _spr = nullptr;
  int _bx() { return 0; }
  int _by() { return 0; }
#else
  int _bx() { return bodyX(); }
  int _by() { return bodyY(); }
#endif
};

#endif
