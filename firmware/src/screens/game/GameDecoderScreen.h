#pragma once

#include "ui/templates/BaseScreen.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)
// ─────────────────────────────────────────────────────────────────────────────
// Decoder — native LVGL build. Timed hex-code crack with Mastermind-style
// marks. Geometry: arena 320×206 — grid 8 rows × 4 cols, cell 36×19 + 3px
// gaps → 173px tall at y=28 (fits with the title row). UP/DOWN cycle the
// hex digit at the cursor (GameNav held-repeat), LEFT/RIGHT move, PRESS
// submit the row, BACK = menu. Difficulty changes the countdown: 90/60/30 s.
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"
#include "ui/lvgl/GameNav.h"

class GameDecoderScreen : public BaseScreen
{
public:
  const char* title()     override { return "Decoder"; }
  bool inhibitPowerSave() override { return _state == ST_PLAY; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_MENU, ST_PLAY, ST_RESULT };

  static constexpr int kCodeLen  = 4;
  static constexpr int kMaxTries = 8;
  static constexpr int kDiffN    = 3;
  static const char* const kDiffNames[kDiffN];
  static constexpr uint32_t kTimeMs[kDiffN] = { 90000, 60000, 30000 };

  State    _state = ST_MENU;
  int8_t   _menuIdx = 0;
  uint8_t  _diff = 0;
  char     _secret[kCodeLen + 1] = {};
  char     _grid[kMaxTries][kCodeLen + 1] = {};
  uint8_t  _marks[kMaxTries][kCodeLen] = {};   // 0 grey 1 yellow 2 green
  uint8_t  _row = 0;
  uint8_t  _col = 0;
  uint8_t  _entry[kCodeLen] = {};              // chosen digit per cell
  uint32_t _endMs = 0;
  int      _best[kDiffN] = {};

  GameNav  _nav;

  lv_obj_t* _arena = nullptr;
  lv_obj_t* _timer = nullptr;
  lv_obj_t* _cells[kMaxTries][kCodeLen] = {};
  lv_obj_t* _overlay = nullptr;
  lv_obj_t* _ovRows[4] = {};

  void _build();
  void _showMenu();
  void _showBest();
  void _hideOverlay();
  void _showResult(bool win);
  void _startPlay();
  void _refreshRow(int row);
  void _markRow(int row);
  void _submitRow();
  void _paintCell(int r, int c);
};

#else  // legacy — original from the repo

// ── Legacy: original header from the repository (verbatim) ──

#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/SpriteCanvas.h"
#endif

class GameDecoderScreen : public BaseScreen
{
public:
  const char* title()        override { return "HEX Decoder"; }
  bool inhibitPowerSave()    override { return _state == STATE_PLAY; }
  bool inhibitPowerOff()     override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { STATE_MENU, STATE_PLAY, STATE_RESULT, STATE_HIGH_SCORES } _state = STATE_MENU;

  static constexpr uint8_t kInputLen   = 4;
  static constexpr uint8_t kMaxHistory = 14;  // max attempts across all difficulties (Easy=14)
  static constexpr uint8_t kCharDBLen  = 10;
  static constexpr uint8_t kMaxScores  = 5;
  static constexpr uint8_t kMenuItems  = 4;
  static constexpr uint8_t kDiffCount  = 4;

  struct Score { int turns; uint32_t ms; };

  // Main menu
  int8_t  _menuIdx    = 0;
  uint8_t _difficulty = 0;
  uint8_t _hsViewDiff = 0;

  // Game state
  char     _target[kInputLen]                = {};
  char     _current[kInputLen]               = {};
  int8_t   _cursor      = 0;   // active block 0–3
  int8_t   _cycleIdx    = 0;   // char index in kCharDB (non-keyboard nav)

  char     _history[kMaxHistory][kInputLen]  = {};
  uint8_t  _histSize    = 0;

  uint8_t  _totalInputs  = 0;
  uint32_t _startMs      = 0;
  uint32_t _endMs        = 0;
  uint32_t _lastRenderMs = 0;
  bool     _win          = false;

  // Scores
  Score   _scores[kMaxScores] = {};
  uint8_t _scoreCount = 0;
  int8_t  _newRank    = -1;

  // Partial-redraw tracking
  State   _prevState      = (State)0xFF;
  int8_t  _lastMenuIdx    = -1;
  uint8_t _lastDifficulty = 0xFF;
  uint8_t _lastHsViewDiff = 0xFF;
  uint8_t _lastTotalInputs = 0xFF;
  int     _lastTimerSec   = -1;

  // Helpers
  const char* _diffStr()              const;
  int         _timerSecs()            const;
  int         _maxAttempts()          const;
  int         _colorGuess(uint8_t idx, char c) const;

  void _initGame();
  void _pushHistory();
  void _submitGuess();
  void _backChar();           // cursor-- or exit to menu
  void _confirmChar();        // non-keyboard: lock cycleIdx char into _current
  void _handleKeyInput(char c);

  void _loadScores(uint8_t diff);
  void _saveScores(uint8_t diff);
  void _insertScore(int turns, uint32_t ms);

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
