#pragma once

#include "ui/templates/BaseScreen.h"

#ifdef USE_LVGL_SHELL
// ─────────────────────────────────────────────────────────────────────────────
// Memory Sequence — native LVGL build. Panels/labels only, no sprites.
// Rules preserved: per-difficulty stage configs (EASY..EXTREME), 3 mistakes,
// rounds grow the sequence, score by stage multiplier, bests in Config.
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"

class GameMemoryScreen : public BaseScreen
{
public:
  const char* title()       override { return "Memory Sequence"; }
  bool inhibitPowerSave()    override { return true; }
  bool inhibitPowerOff()     override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_MENU, ST_SHOW, ST_INPUT, ST_OK, ST_BAD, ST_LOSS, ST_SCORES };

  static constexpr uint8_t kMaxSeq = 7;
  static constexpr uint8_t kMenuN  = 4;   // Play / Difficulty / Best / Exit
  static constexpr uint8_t kDiffN  = 4;

  struct Stage { uint8_t len; uint16_t showMs; uint8_t mult; };
  static constexpr Stage kStages[kDiffN] = {
    { 3, 900, 1 }, { 4, 700, 2 }, { 5, 450, 4 }, { 7, 260, 8 },
  };
  static const char* const kDiffNames[kDiffN];

  State   _state = ST_MENU;
  int8_t  _menuIdx = 0;
  uint8_t _difficulty = 0;

  char    _seq[kMaxSeq + 1] = {};
  uint8_t _seqLen = 0;
  uint8_t _pos = 0;          // input position
  char    _cycleCh = 'A';    // cycled char at cursor
  uint16_t _round = 1;
  uint32_t _score = 0;
  uint8_t  _mistakes = 0;
  uint32_t _showUntil = 0;
  uint32_t _flashUntil = 0;
  uint16_t _bestRound[kDiffN] = {};
  bool     _okSoFar = true;

  // widgets
  lv_obj_t* _arena  = nullptr;
  lv_obj_t* _title  = nullptr;
  lv_obj_t* _status = nullptr;   // round / score / mistakes line
  lv_obj_t* _board  = nullptr;   // sequence cells
  lv_obj_t* _cells[kMaxSeq] = {};
  lv_obj_t* _hint = nullptr;
  lv_obj_t* _overlay = nullptr;  // menu / result / scores
  lv_obj_t* _ovRows[6] = {};
  uint8_t  _ovN = 0;

  void _build();
  void _showMenu();
  void _hideOverlay();
  void _showLoss();
  void _showScores();
  void _newRound();
  void _advancePhase();
  void _confirm();
  void _backspace();
  void _refreshBoard(bool reveal);
};

#else  // legacy TFT — original implementation from the repo

// ── Legacy: original header from the repository (verbatim) ──

#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/SpriteCanvas.h"
#endif

class GameMemoryScreen : public BaseScreen
{
public:
  const char* title()       override { return "Memory Sequence"; }
  bool inhibitPowerSave()   override { return _state == STATE_SHOW_SEQUENCE || _state == STATE_WAITING_INPUT; }
  bool inhibitPowerOff()    override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State {
    STATE_MENU,
    STATE_SHOW_SEQUENCE,
    STATE_WAITING_INPUT,
    STATE_FEEDBACK_CORRECT,
    STATE_FEEDBACK_WRONG,
    STATE_GAME_LOSS,
    STATE_HIGH_SCORES
  } _state = STATE_MENU;

  static constexpr uint8_t kMaxSeqLen  = 7;
  static constexpr uint8_t kCharDBLen  = 36;
  static constexpr uint8_t kDiffCount  = 4;
  static constexpr uint8_t kMenuItems  = 4;

  struct StageConfig {
    uint8_t  length;
    uint16_t displayTimeMs;
    uint8_t  multiplier;
  };

  struct HighScore {
    uint16_t round;
    uint32_t score;
  };

  // Menu
  int8_t  _menuIdx    = 0;
  uint8_t _difficulty = 0;   // 0=EASY 1=MEDIUM 2=HARD 3=EXTREME
  uint8_t _hsViewIdx  = 0;   // which difficulty shown in high scores screen

  // Game state
  char     _sequence[kMaxSeqLen] = {};
  char     _current[kMaxSeqLen]  = {};
  uint8_t  _seqLen          = 0;
  int8_t   _cursor          = 0;
  int8_t   _cycleIdx        = 0;
  uint16_t _round           = 1;
  uint32_t _score           = 0;
  uint8_t  _mistakes        = 0;
  uint8_t  _maxMistakes     = 3;
  uint8_t  _streak          = 0;
  bool     _roundHadMistake = false;
  bool     _isNewHigh       = false;
  uint32_t _lastPointsEarned = 0;

  // Timing
  uint32_t _stateTimer    = 0;
  uint32_t _lastRenderMs  = 0;

  // Partial-redraw tracking
  State   _prevState       = (State)0xFF;
  int8_t  _lastMenuIdx     = -1;
  uint8_t _lastDifficulty  = 0xFF;
  uint8_t _lastHsViewIdx   = 0xFF;
  int8_t  _lastSeqSlot     = -1;
  bool    _lastSeqShowing  = false;
  bool    _feedbackDrawn   = false;
  int8_t  _lastCursor      = -1;
  int8_t  _lastCycleIdx    = -1;
  uint8_t _lastMistakes    = 0xFF;
  uint32_t _lastScoreCache = 0xFFFFFFFFu;
  uint16_t _lastRoundCache = 0xFFFF;

  // High scores
  HighScore _highScores[kDiffCount] = {};

  // Helpers
  StageConfig  _getStage()            const;
  const char*  _diffStr()             const;
  const char*  _diffStrShort()        const;
  uint8_t      _getMaxMistakes()      const;
  void         _initGame();
  void         _newRound();
  void         _submitGuess();
  void         _backChar();
  void         _confirmChar();
  void         _handleKeyInput(char c);
  void         _loadHighScores();
  void         _saveHighScore();

  void _renderMenu();
  void _renderShowSequence();
  void _renderWaitingInput();
  void _renderFeedbackCorrect();
  void _renderFeedbackWrong();
  void _renderGameLoss();
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
