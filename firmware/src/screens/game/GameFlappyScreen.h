#pragma once

#include "ui/templates/BaseScreen.h"

#ifdef USE_LVGL_SHELL
// ─────────────────────────────────────────────────────────────────────────────
// Flappy Bird — native LVGL build (USE_LVGL_SHELL boards).
//
// No sprites, no canvas, no per-frame byte swaps: the bird is a rounded lv_obj,
// pipes are lv_obj rectangles, score is a label. Motion = lv_obj_set_pos per
// physics tick (LVGL invalidates only the moved regions). This sidesteps the
// SpriteCanvas full-frame pipeline entirely — the class of hard hangs the
// sprite-port games showed.
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"

class GameFlappyScreen : public BaseScreen
{
public:
  const char* title()     override { return "Flappy Bird"; }
  bool inhibitPowerSave() override { return _state == STATE_PLAY; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { STATE_MENU, STATE_PLAY, STATE_RESULT, STATE_SCORES };

  // ── physics (same constants as the legacy build) ───────────────────────
  static constexpr float    kGravity   = 0.35f;
  static constexpr float    kFlapForce = -3.2f;
  static constexpr uint8_t  kGroundH  = 6;
  static constexpr uint8_t  kBirdW     = 12;
  static constexpr uint8_t  kBirdH     = 10;
  static constexpr uint8_t  kPipeW     = 16;
  static constexpr uint8_t  kMaxPipes  = 4;
  static constexpr int      kBirdX     = 16;

  struct Pipe { int16_t x; uint8_t gapY; bool scored; };
  Pipe _pipes[kMaxPipes] = {};
  uint8_t _pipeCount = 0;

  State   _state = STATE_MENU;
  int8_t  _menuIdx = 0;
  float   _birdY = 0;
  float   _velocity = 0;
  int     _score = 0;
  int     _best  = 0;
  uint32_t _lastTickMs = 0;
  uint32_t _pipeTimer = 0;
  uint8_t  _gapH = 0;
  uint8_t  _pipeSpeed = 0;
  uint16_t _pipeInterval = 0;
  int16_t  _arenaW = 0;
  int16_t  _arenaH = 0;

  // ── widgets ──────────────────────────────────────────────────────────────
  lv_obj_t* _arena   = nullptr;   // play field on viewParent()
  lv_obj_t* _bird    = nullptr;
  lv_obj_t* _pipeT[kMaxPipes] = {};
  lv_obj_t* _pipeB[kMaxPipes] = {};
  lv_obj_t* _ground  = nullptr;
  lv_obj_t* _scoreL  = nullptr;
  lv_obj_t* _overlay = nullptr;   // menu / result / scores panel
  lv_obj_t* _ovRows[6] = {};
  uint8_t  _ovRowCount = 0;

  void _buildArena();
  void _showMenu();
  void _showResult();
  void _showScores();
  void _hideOverlay();
  void _startPlay();
  void _tick();
  void _flap();
  void _spawnPipe();
  bool _collision() const;
  void _syncWidgets();   // physics state → widget pos/size
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
