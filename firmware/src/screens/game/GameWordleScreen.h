#pragma once

#include "ui/templates/BaseScreen.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)
// ─────────────────────────────────────────────────────────────────────────────
// Wordle — native LVGL build. 5×6 key-cap grid, stick-only: UP/DOWN cycle the
// letter at the cursor (GameNav held-repeat), LEFT/RIGHT move, PRESS submit,
// BACK = exit. Standard marking (green exact / yellow misplaced / grey absent)
// with a consumed-letter pass. EN + ID databases share the class.
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"
#include "ui/lvgl/GameNav.h"

class GameWordleScreen : public BaseScreen
{
public:
  enum Language : uint8_t { LANG_EN = 0, LANG_ID = 1 };

  explicit GameWordleScreen(Language lang) : _lang(lang) {}

  const char* title()     override { return "Wordle"; }
  bool inhibitPowerSave() override { return _state == ST_PLAY; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_PLAY, ST_END };

  static constexpr int kWordLen  = 5;
  static constexpr int kMaxTries = 6;

  Language _lang;
  State    _state = ST_PLAY;
  char     _target[kWordLen + 1] = {};
  char     _grid[kMaxTries][kWordLen + 1] = {};    // submitted rows (uppercase)
  uint8_t  _marks[kMaxTries][kWordLen] = {};
  uint8_t  _row = 0;
  uint8_t  _col = 0;
  char     _entry[kWordLen] = { 'A','A','A','A','A' };   // letter per cell

  GameNav  _nav;

  lv_obj_t* _arena = nullptr;
  lv_obj_t* _cells[kMaxTries][kWordLen] = {};
  lv_obj_t* _hint = nullptr;
  lv_obj_t* _overlay = nullptr;

  void _build();
  void _pickWord();
  void _refreshRow(int row);
  void _markRow(int row);
  void _submitRow();
  void _showEnd(bool win);
  void _hideOverlay();
  void _reset();
};

#else  // legacy — original implementation from the repo

// ── Legacy: original header from the repository (verbatim) ──

#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/SpriteCanvas.h"
#endif

class GameWordleScreen : public BaseScreen
{
public:
  enum Language : uint8_t { LANG_EN = 0, LANG_ID = 1 };

  GameWordleScreen(Language lang = LANG_EN) : _language(lang) {}

  const char* title()         override { return _language == LANG_ID ? "Wordle ID" : "Wordle EN"; }
  bool inhibitPowerSave()     override { return _state == STATE_PLAY; }
  bool inhibitPowerOff()      override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { STATE_MENU, STATE_PLAY, STATE_RESULT, STATE_HIGH_SCORES } _state = STATE_MENU;

  static constexpr uint8_t kWordLen    = 5;
  static constexpr uint8_t kMaxHistory = 10;
  static constexpr uint8_t kAlphaLen   = 26;
  static constexpr uint8_t kMaxScores  = 5;
  static constexpr uint8_t kMenuItems  = 5;
  static constexpr uint8_t kDiffCount  = 3;

  struct Score { int turns; uint32_t ms; };

  Language _language;

  // Menu
  int8_t  _menuIdx    = 0;
  uint8_t _difficulty = 0;   // 0=Easy 1=Medium 2=Hard
  uint8_t _hsViewDiff = 0;
  bool    _useCommon  = true;

  // Game state
  char     _target[kWordLen]                       = {};
  char     _current[kWordLen]                      = {};
  int8_t   _cursor   = 0;
  int8_t   _cycleIdx = 0;

  char     _history[kMaxHistory][kWordLen]         = {};
  uint8_t  _histSize    = 0;
  uint8_t  _totalInputs = 0;
  uint8_t  _alphabetUsed[kAlphaLen]               = {};  // 0=not tried, 1=tried
  bool     _win         = false;
  uint32_t _startMs     = 0;

  // Scores
  Score   _scores[kMaxScores] = {};
  uint8_t _scoreCount = 0;
  int8_t  _newRank    = -1;

  // Partial-redraw tracking
  State   _prevState      = (State)0xFF;
  int8_t  _lastMenuIdx    = -1;
  uint8_t _lastDifficulty = 0xFF;
  uint8_t _lastHsViewDiff = 0xFF;
  int8_t  _lastUseCommon  = -1;

  // Helpers
  const char* _diffStr()    const;
  int         _maxAttempts() const;
  int         _colorGuess(uint8_t idx, char c) const;

  void _initGame();
  void _pushHistory();
  void _submitGuess();
  void _backChar();
  void _confirmChar();
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
  Sprite* _lvSpr() { return _spr; }
#else
  int _bx() { return bodyX(); }
  int _by() { return bodyY(); }
  Sprite* _lvSpr();   // defined in the cpp: the panel
#endif
};

#endif
