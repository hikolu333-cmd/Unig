#pragma once

#include "ui/templates/BaseScreen.h"

#ifdef USE_LVGL_SHELL
// ─────────────────────────────────────────────────────────────────────────────
// Quenswer — native LVGL build. Two-player letter quiz: P1 sets the answer
// (stick char-cycler) and an attempt budget, P2 guesses letter by letter
// (hangman-style reveals). Longer answers + tighter budgets score higher.
// ─────────────────────────────────────────────────────────────────────────────
#include <lvgl.h>
#include "ui/lvgl/LvglUi.h"

class GameQuenswerScreen : public BaseScreen
{
public:
  const char* title()     override { return "Quenswer"; }
  bool inhibitPowerSave() override { return true; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State { ST_SETUP, ST_HANDOFF, ST_PLAY, ST_RESULT };

  static constexpr uint8_t kMaxLen = 14;
  static constexpr uint8_t kMaxTries = 6;

  State   _state = ST_SETUP;
  char    _answer[kMaxLen + 1] = {};
  uint8_t _answerLen = 0;
  char    _revealed[kMaxLen + 1] = {};
  uint8_t _pos = 0;         // setup cursor
  char    _letter = 'A';    // cycled char
  uint8_t _budget = 6;      // attempts for P2
  uint8_t _used = 0;
  uint32_t _flashUntil = 0;
  int      _bestScore = 0;

  lv_obj_t* _arena = nullptr;
  lv_obj_t* _title = nullptr;
  lv_obj_t* _cellsCont = nullptr;
  lv_obj_t* _cells[kMaxLen] = {};
  lv_obj_t* _status = nullptr;
  lv_obj_t* _hint = nullptr;
  lv_obj_t* _overlay = nullptr;
  lv_obj_t* _ovTitle = nullptr;
  lv_obj_t* _ovSub = nullptr;

  void _build();
  void _refreshCells();
  void _cycle(int dir);
  void _confirmChar();
  void _backspace();
  void _finishSetup();
  void _showHandoff();
  void _showResult(bool solved);
  void _hideOverlay();
  void _guess();
};

#else  // legacy — original from the repository

// ── Legacy: original header from the repository (verbatim) ──

#include "ui/templates/BaseScreen.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/SpriteCanvas.h"
#endif

// "Quenswer" — a two-player pass-the-device guessing game.
//
//  Player 1 sets a clue, a hidden answer (max 20 chars) and an attempt budget,
//  then hands the device to Player 2, who guesses the answer letter by letter.
//  Correctly placed letters stay locked between attempts to ease the hunt.
//  Solving scores higher the longer the answer and the tighter the budget.
class GameQuenswerScreen : public BaseScreen
{
public:
  const char* title()     override { return "Quenswer"; }
  bool inhibitPowerSave() override { return _state == STATE_PLAY; }
  bool inhibitPowerOff()  override { return true; }

  void onInit()   override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State : uint8_t { STATE_SETUP, STATE_HANDOFF, STATE_PLAY, STATE_RESULT } _state = STATE_SETUP;

  static constexpr uint8_t kMaxLen     = 20;
  static constexpr uint8_t kSetupRows  = 5;  // Clue / Answer / Attempts / Start / Exit
  static constexpr uint8_t kAlphaLen   = 26; // cycler letters (index 26 == erase)
  static constexpr uint8_t kMaxHistory = 8;  // recent guesses shown below the blanks

  // ── Configuration (Player 1) ───────────────────────────────
  String  _clue;
  char    _answer[kMaxLen + 1] = {};   // uppercased target
  uint8_t _answerLen   = 0;
  uint8_t _maxAttempts = 6;            // 0 == unlimited
  int8_t  _setupIdx    = 0;

  // ── Play state (Player 2) ──────────────────────────────────
  bool    _locked[kMaxLen] = {};       // position revealed (non-letter) or solved
  char    _guess[kMaxLen]  = {};       // working guess; '\0' == blank
  uint8_t _editableCount   = 0;        // number of guessable (letter) positions
  uint8_t _used            = 0;        // attempts made
  int8_t  _cycleIdx        = 0;        // non-keyboard letter picker
  bool    _win             = false;
  long    _score           = 0;

  // Recent guesses (most recent first) shown below the answer blanks.
  char    _history[kMaxHistory][kMaxLen + 1] = {};
  uint8_t _histCount = 0;

  // ── Helpers ────────────────────────────────────────────────
  static bool _isLetter(char c) { return c >= 'A' && c <= 'Z'; }
  int  _firstEmptyEditable() const;
  int  _lastFilledEditable() const;
  bool _anyFilled() const { return _lastFilledEditable() >= 0; }
  long _computeScore() const;
  const char* _rankTitle() const;
  uint16_t _histColor(char c, uint8_t pos) const;   // green/orange/grey feedback
  void _pushHistory(const char* word);

  void _editClue();
  void _editAnswer();
  void _editAttempts();
  void _startGame();
  void _submit();
  void _typeChar(char c);
  void _confirmCycle();
  void _eraseLast();

  void _renderSetup();
  void _renderHandoff();
  void _renderPlay();
  void _renderResult();

#ifdef USE_LVGL_SHELL
  // Every state renders the full body into a sprite — bridged via canvas.
  SpriteCanvas _scene;
#endif
};

#endif
