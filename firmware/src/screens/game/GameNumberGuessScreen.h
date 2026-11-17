#pragma once

#include "ui/templates/BaseScreen.h"

#ifdef USE_LVGL_SHELL
// ─────────────────────────────────────────────────────────────────────────────
// Number Guess — native LVGL build. Big value label, hint line (higher/lower),
// UP/DOWN = ±1, LEFT/RIGHT = ±10, PRESS = submit, unlimited/limited by stage.
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"

class GameNumberGuessScreen : public BaseScreen
{
public:
  const char* title()     override { return "Number Guess"; }
  bool inhibitPowerSave() override { return true; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_MENU, ST_PLAY, ST_RESULT, ST_SCORES };

  static constexpr uint8_t kDiffN = 4;
  static const char* const kDiffNames[kDiffN];
  static constexpr int kMaxVal[kDiffN]  = { 99, 999, 9999, 9999 };
  static constexpr int kTrials[kDiffN]  = { 0, 8, 10, 12 };   // 0 = unlimited
  static constexpr uint8_t kMenuN = 4;

  State   _state = ST_MENU;
  int8_t  _menuIdx = 0;
  uint8_t _diff = 0;
  int     _target = 0;
  int     _guess  = 0;
  int     _tries  = 0;
  bool    _lastHigh = false;   // direction of the last hint
  int     _best[kDiffN] = {};

  lv_obj_t* _arena = nullptr;
  lv_obj_t* _valueLbl = nullptr;
  lv_obj_t* _hintLbl  = nullptr;
  lv_obj_t* _triesLbl = nullptr;
  lv_obj_t* _overlay = nullptr;
  lv_obj_t* _ovRows[kMenuN] = {};
  uint8_t  _ovN = 0;

  void _build();
  void _showMenu();
  void _hideOverlay();
  void _showResult(bool win);
  void _showScores();
  void _startPlay();
  void _submit();
  void _paintValue();
};

#else  // legacy — original repo implementation

// ── Legacy: original header from the repository (verbatim) ──

#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/SpriteCanvas.h"
#endif

class GameNumberGuessScreen : public BaseScreen
{
public:
  const char* title()       override { return "Number Guess"; }
  bool inhibitPowerSave()   override { return _state == STATE_PLAY; }
  bool inhibitPowerOff()    override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State {
    STATE_MENU,
    STATE_PLAY,
    STATE_WIN,
    STATE_LOSE,
    STATE_HIGH_SCORES
  } _state = STATE_MENU;

  static constexpr uint8_t kDiffCount = 4;
  static constexpr uint8_t kMenuItems = 4;
  static constexpr uint8_t kMaxScores = 5;

  struct Score {
    int      guesses;
    uint32_t ms;
  };

  // Menu
  int8_t  _menuIdx    = 0;
  uint8_t _diff       = 0;
  uint8_t _hsViewDiff = 0;

  // Game state
  int      _target     = 0;
  int      _lastGuess  = -1;
  int      _guesses    = 0;
  int      _trialsLeft = 0;
  uint32_t _startMs    = 0;
  int8_t   _cmp        = 0;    // -1=lower, 0=none, 1=higher
  bool     _hasGuessed = false;

  // Digit input
  char    _inputBuf[6] = {};   // max 4 digits + null
  uint8_t _inputCursor = 0;
  int8_t  _cycleDigit  = 0;   // non-keyboard: 0-9=digit, 10=backspace '<'
  bool    _holdFired   = false;

  // Scores for current difficulty
  Score   _scores[kMaxScores] = {};
  uint8_t _scoreCount = 0;
  int8_t  _newRank    = -1;

  // Partial-redraw tracking
  State   _prevState      = (State)0xFF;
  int8_t  _lastMenuIdx    = -1;
  uint8_t _lastDiff       = 0xFF;
  uint8_t _lastHsViewDiff = 0xFF;

  uint8_t _maxDigits()  const;

  void _initGame();
  void _submitGuess();
  void _confirmDigit();      // non-keyboard: lock in cycled digit
  void _backDigit();
  void _loadScores(uint8_t diff);
  void _saveScores(uint8_t diff);
  void _insertScore(int g, uint32_t ms);

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
