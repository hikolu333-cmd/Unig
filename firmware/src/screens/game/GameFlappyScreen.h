#pragma once

#include "ui/templates/BaseScreen.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)
// ─────────────────────────────────────────────────────────────────────────────
// Flappy — native LVGL build. Composite-object bird (body/wing/eye/beak),
// pipes with caps, drifting clouds, sandy ground. Widget positions are
// synced from the physics state EVERY frame in onUpdate() — onRender() is
// not called per-loop in the LVGL shell, so onRender() must not carry the
// sync (that was the frozen-pipes bug of the first port).
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"
#include "ui/lvgl/GameNav.h"

class GameFlappyScreen : public BaseScreen
{
public:
  const char* title()     override { return "Flappy"; }
  bool inhibitPowerSave() override { return _state == ST_PLAY; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_MENU, ST_PLAY, ST_RESULT, ST_SCORES };
  struct Pipe { int16_t x; int16_t gapY; bool scored; };

  static constexpr float   kGravity   = 0.35f;
  static constexpr float   kFlapForce = -3.2f;
  static constexpr int     kGroundH   = 14;
  static constexpr int     kPipeW     = 26;
  static constexpr int     kCapW      = 32;
  static constexpr int     kCapH      = 10;
  static constexpr int     kMaxPipes  = 4;
  static constexpr int     kBirdX     = 52;
  static constexpr int     kBirdW     = 18;
  static constexpr int     kBirdH     = 14;
  static constexpr int     kTickMs    = 33;
  static constexpr int     kMaxClouds = 3;

  State    _state = ST_MENU;
  int8_t   _menuIdx = 0;
  int      _best   = 0;
  int      _score  = 0;

  int16_t  _arenaW = 320, _arenaH = 206;
  int      _gapH  = 70;
  int      _pipeSpeed   = 2;
  int      _pipeInterval = 1600;
  int      _pipeTimer   = 0;
  uint32_t _lastTickMs  = 0;
  float    _birdY  = 100.0f;
  float    _vel    = 0.0f;
  uint8_t  _pipeCount = 0;
  Pipe     _pipes[kMaxPipes] = {};

  GameNav  _nav;

  lv_obj_t* _arena  = nullptr;
  lv_obj_t* _clouds[kMaxClouds] = {};
  // Slot size latches: a pipe's gapY is fixed for its life, so the shaft
  // resize happens once per pipe instead of every frame.
  bool     _sizedT[kMaxPipes] = {};
  bool     _sizedB[kMaxPipes] = {};
  lv_obj_t* _pipeT[kMaxPipes] = {};   // upper pipe shaft
  lv_obj_t* _pipeTC[kMaxPipes] = {};  // upper pipe cap (at the gap)
  lv_obj_t* _pipeB[kMaxPipes] = {};   // lower pipe shaft
  lv_obj_t* _pipeBC[kMaxPipes] = {};  // lower pipe cap (at the gap)
  lv_obj_t* _ground = nullptr;
  lv_obj_t* _bird   = nullptr;         // transparent group holding body parts
  lv_obj_t* _scoreL = nullptr;
  lv_obj_t* _overlay = nullptr;
  lv_obj_t* _ovRows[3] = {};

  void _buildArena();
  void _showMenu();
  void _showScores();
  void _showResult();
  void _hideOverlay();
  void _startPlay();
  void _flap();
  void _spawnPipe();
  bool _collision() const;
  void _tick();
  void _sync();
  void _menuFocus();
  void _menuRows(const char* const* rows, int n);
};

#else  // legacy TFT boards — original implementation

// ── Legacy: original header from the repository (verbatim) ──

#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/SpriteCanvas.h"
#endif

class GameFlappyScreen : public BaseScreen
{
public:
  const char* title()        override { return "Flappy Bird"; }
  bool inhibitPowerSave()    override { return _state == STATE_PLAY; }
  bool inhibitPowerOff()     override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { STATE_MENU, STATE_PLAY, STATE_RESULT, STATE_HIGH_SCORES } _state = STATE_MENU;

  static constexpr uint8_t kBirdW     = 8;
  static constexpr uint8_t kBirdH     = 6;
  static constexpr uint8_t kPipeW     = 12;
  static constexpr uint8_t kMaxPipes  = 4;
  static constexpr uint8_t kTopN      = 5;
  static constexpr uint8_t kMenuItems = 3;

  // Menu
  int8_t _menuIdx = 0;

  // Bird
  float _birdY    = 0;
  float _velocity = 0;

  // Pipes
  struct Pipe {
    int16_t x;
    int16_t prevX;
    uint8_t gapY;
    bool    scored;
  };
  Pipe    _pipes[kMaxPipes] = {};
  uint8_t _pipeCount = 0;

  // Game state
  int      _score      = 0;
  int      _lastScore  = 0;
  int      _bestScore  = 0;
  bool     _isNewHigh  = false;
  uint32_t _lastFrameMs = 0;
  uint32_t _pipeTimer   = 0;

  // High scores
  int _topScores[kTopN] = {};

  // Partial-redraw tracking
  State   _prevState   = (State)0xFF;
  int8_t  _lastMenuIdx = -1;

  // Play-state dirty tracking
  int16_t _prevBirdY = -1;

  // FPS counter
  uint32_t _fpsTimer = 0;
  uint8_t  _fpsCount = 0;
  uint8_t  _fps      = 0;

  // Derived from screen size
  uint8_t _gapH      = 0;
  uint8_t _pipeSpeed = 0;
  uint16_t _pipeInterval = 0;

#ifdef DEVICE_HAS_TOUCH_NAV
  static constexpr uint16_t kTouchFirstRepeatMs = 350;
  static constexpr uint16_t kTouchRepeatMs      = 200;
  bool     _prevTouching  = false;
  uint32_t _touchRepeatAt = 0;
#endif

  void _initGame();
  void _flap();
  void _updateGame();
  bool _checkCollision();
  void _spawnPipe();
  void _loadHighScores();
  void _saveHighScore();

  void _renderMenu();
  void _renderPlay();
  void _renderResult();
  void _renderHighScores();

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
