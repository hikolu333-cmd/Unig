//
// Flappy — native LVGL build (USE_LVGL_SHELL). The bird is a small group of
// basic objects (body / wing / eye / beak), pipes are shaft+cap pairs in
// bright green, clouds drift behind everything. Positions sync from physics
// state every onUpdate frame.
//

#include "GameFlappyScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"

#if defined(USE_LVGL_SHELL) && !defined(GAME_TFT_CANVAS)

void GameFlappyScreen::onInit()
{
  _best = Config.get("flappy_best", "0").toInt();
  _buildArena();
  _showMenu();
}

void GameFlappyScreen::_buildArena()
{
  lv_obj_t* parent = LvglUi::viewParent();
  _arena = gamekit::arena(parent, 0x0f1626);          // night sky (dark theme)
  _arenaW = lv_obj_get_width(_arena);
  _arenaH = lv_obj_get_height(_arena);
  if (_arenaW < 100) _arenaW = 320;
  if (_arenaH < 100) _arenaH = 206;

  // clouds (below everything else — created first)
  for (int i = 0; i < kMaxClouds; i++) {
    lv_obj_t* cl = lv_obj_create(_arena);
    lv_obj_set_pos(cl, 40 + i * 110, 18 + (i % 2) * 26);
    lv_obj_set_size(cl, 40 + (i % 2) * 10, 12);
    lv_obj_set_style_bg_color(cl, lv_color_hex(0x24304c), 0);
    lv_obj_set_style_bg_opa(cl, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cl, 0, 0);
    lv_obj_set_style_radius(cl, LV_RADIUS_CIRCLE, 0);
    _clouds[i] = cl;
  }

  // pipes: shaft + cap pairs
  for (int i = 0; i < kMaxPipes; i++) {
    auto pipePart = [&](lv_obj_t** shaft, lv_obj_t** cap) {
      *shaft = lv_obj_create(_arena);
      lv_obj_set_style_bg_color(*shaft, lv_color_hex(0x43a047), 0);
      lv_obj_set_style_bg_opa(*shaft, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(*shaft, lv_color_hex(0x1b5e20), 0);
      lv_obj_set_style_border_width(*shaft, 2, 0);
      lv_obj_set_style_radius(*shaft, 0, 0);
      lv_obj_add_flag(*shaft, LV_OBJ_FLAG_HIDDEN);

      *cap = lv_obj_create(_arena);
      lv_obj_set_size(*cap, kCapW, kCapH);
      lv_obj_set_style_bg_color(*cap, lv_color_hex(0x4caf50), 0);
      lv_obj_set_style_bg_opa(*cap, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(*cap, lv_color_hex(0x1b5e20), 0);
      lv_obj_set_style_border_width(*cap, 2, 0);
      lv_obj_set_style_radius(*cap, 3, 0);
      lv_obj_add_flag(*cap, LV_OBJ_FLAG_HIDDEN);
    };
    pipePart(&_pipeT[i], &_pipeTC[i]);
    pipePart(&_pipeB[i], &_pipeBC[i]);
  }

  // ground strip
  _ground = lv_obj_create(_arena);
  lv_obj_set_pos(_ground, 0, _arenaH - kGroundH);
  lv_obj_set_size(_ground, _arenaW, kGroundH);
  lv_obj_set_style_bg_color(_ground, lv_color_hex(0x3d3226), 0);
  lv_obj_set_style_bg_opa(_ground, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(_ground, lv_color_hex(0x8b6f47), 0);
  lv_obj_set_style_border_width(_ground, 2, 0);
  lv_obj_set_style_border_side(_ground, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_radius(_ground, 0, 0);

  // bird group: transparent carrier + parts
  _bird = lv_obj_create(_arena);
  lv_obj_set_pos(_bird, kBirdX, (int)_birdY);
  lv_obj_set_size(_bird, kBirdW + 6, kBirdH);
  lv_obj_set_style_bg_opa(_bird, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_bird, 0, 0);
  lv_obj_set_style_pad_all(_bird, 0, 0);
  lv_obj_clear_flag(_bird, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(_bird, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* body = lv_obj_create(_bird);
  lv_obj_set_pos(body, 0, 0);
  lv_obj_set_size(body, kBirdW, kBirdH);
  lv_obj_set_style_bg_color(body, lv_color_hex(0xffd54f), 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(body, lv_color_hex(0x8d6e00), 0);
  lv_obj_set_style_border_width(body, 2, 0);
  lv_obj_set_style_radius(body, LV_RADIUS_CIRCLE, 0);

  lv_obj_t* wing = lv_obj_create(_bird);
  lv_obj_set_pos(wing, 2, 5);
  lv_obj_set_size(wing, 9, 6);
  lv_obj_set_style_bg_color(wing, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(wing, LV_OPA_90, 0);
  lv_obj_set_style_border_width(wing, 0, 0);
  lv_obj_set_style_radius(wing, 3, 0);

  lv_obj_t* eye = lv_obj_create(_bird);
  lv_obj_set_pos(eye, kBirdW - 6, 2);
  lv_obj_set_size(eye, 4, 4);
  lv_obj_set_style_bg_color(eye, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(eye, 0, 0);
  lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);

  lv_obj_t* beak = lv_obj_create(_bird);
  lv_obj_set_pos(beak, kBirdW - 2, 5);
  lv_obj_set_size(beak, 7, 4);
  lv_obj_set_style_bg_color(beak, lv_color_hex(0xff8f00), 0);
  lv_obj_set_style_bg_opa(beak, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(beak, 0, 0);
  lv_obj_set_style_radius(beak, 2, 0);

  // score — created last so it sits above the pipes
  _scoreL = gamekit::label(_arena, &lv_font_montserrat_20, 0xffffff,
                          LV_ALIGN_TOP_MID, 0, 4, "");
}

// ── overlays ────────────────────────────────────────────────────────────────

void GameFlappyScreen::_hideOverlay()
{
  if (_overlay) { lv_obj_del(_overlay); _overlay = nullptr; }
}

void GameFlappyScreen::_menuRows(const char* const* rows, int n)
{
  for (int i = 0; i < n; i++)
    _ovRows[i] = gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                                LV_ALIGN_TOP_MID, 0, 30 + i * 36, rows[i]);
}

void GameFlappyScreen::_showMenu()
{
  _state = ST_MENU;
  _hideOverlay();
  _menuIdx = 0;

  _overlay = gamekit::panel(_arena, 220, 170);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 8, "FLAPPY");
  const char* rows[3] = { "Play", "Best Score", "Exit" };
  _menuRows(rows, 3);

  lv_obj_add_flag(_bird, LV_OBJ_FLAG_HIDDEN);
  for (int i = 0; i < kMaxPipes; i++) {
    lv_obj_add_flag(_pipeT[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_pipeTC[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_pipeB[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_pipeBC[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text(_scoreL, "");
}

void GameFlappyScreen::_showScores()
{
  _state = ST_SCORES;
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 200, 120);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffd54f,
                 LV_ALIGN_TOP_MID, 0, 10, "BEST SCORE");
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", _best);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 42, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 78, "any key = back");
}

void GameFlappyScreen::_showResult()
{
  _state = ST_RESULT;
  if (_score > _best) {
    _best = _score;
    Config.set("flappy_best", String(_best));
  }
  _hideOverlay();

  _overlay = gamekit::panel(_arena, 240, 150, 0x3a2a35);
  lv_obj_set_style_border_color(_overlay, lv_color_hex(0xff7043), 0);
  gamekit::label(_overlay, &lv_font_montserrat_20, 0xff7043,
                 LV_ALIGN_TOP_MID, 0, 12, "GAME OVER");
  char buf[48];
  snprintf(buf, sizeof(buf), "Score %d    Best %d", _score, _best);
  gamekit::label(_overlay, &lv_font_montserrat_16, 0xffffff,
                 LV_ALIGN_TOP_MID, 0, 50, buf);
  gamekit::label(_overlay, &lv_font_montserrat_14, 0xcfd8e3,
                 LV_ALIGN_TOP_MID, 0, 90, "PRESS = retry    BACK = menu");
}

// ── gameplay ─────────────────────────────────────────────────────────────────

void GameFlappyScreen::_startPlay()
{
  int field = _arenaH - kGroundH;
  _gapH         = max(56, field / 3);
  _pipeSpeed    = 2;
  _pipeInterval = 1600;
  _birdY        = field / 2.0f;
  _vel          = 0;
  _score        = 0;
  _pipeCount    = 0;
  _pipeTimer    = 0;
  _lastTickMs   = millis();
  _state        = ST_PLAY;

  _hideOverlay();
  lv_obj_clear_flag(_bird, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(_scoreL, "0");

  int n = Achievement.inc("flappy_first_play");
  if (n == 1) Achievement.unlock("flappy_first_play");

  _sync();
}

void GameFlappyScreen::_flap()
{
  _vel = kFlapForce;
  if (Uni.Speaker) Uni.Speaker->playRandomTone(0, 30);
}

void GameFlappyScreen::_spawnPipe()
{
  if (_pipeCount >= kMaxPipes) return;
  int field = _arenaH - kGroundH;
  int minGY = _gapH / 2 + 8;
  int maxGY = field - _gapH / 2 - 8;
  _pipes[_pipeCount++] = { (int16_t)(_arenaW + kCapW), (int16_t)(minGY + random(maxGY - minGY + 1)), false };
}

bool GameFlappyScreen::_collision() const
{
  int field = _arenaH - kGroundH;
  int by = (int)_birdY;
  int bx = kBirdX + 3;                     // body x inside the group
  if (by <= 0 || by + kBirdH >= field) return true;

  for (int i = 0; i < _pipeCount; i++) {
    int px = _pipes[i].x;
    if (bx + kBirdW <= px || bx >= px + kPipeW) continue;
    int gapTop = _pipes[i].gapY - _gapH / 2;
    int gapBot = _pipes[i].gapY + _gapH / 2;
    if (by < gapTop || by + kBirdH > gapBot) return true;
  }
  return false;
}

void GameFlappyScreen::_tick()
{
  _vel += kGravity;
  _birdY += _vel;

  for (int i = 0; i < _pipeCount; i++) {
    _pipes[i].x -= _pipeSpeed;
    if (!_pipes[i].scored && _pipes[i].x + kPipeW < kBirdX) {
      _pipes[i].scored = true;
      _score++;
      char buf[12];
      snprintf(buf, sizeof(buf), "%d", _score);
      lv_label_set_text(_scoreL, buf);
      if (Uni.Speaker && !Uni.Speaker->isPlaying()) Uni.Speaker->playRandomTone(0, 50);
      if (_score == 10)  Achievement.unlock("flappy_score_10");
      if (_score == 25)  Achievement.unlock("flappy_score_25");
      if (_score == 50)  Achievement.unlock("flappy_score_50");
      if (_score == 100) Achievement.unlock("flappy_score_100");
    }
  }

  while (_pipeCount > 0 && _pipes[0].x < -kCapW - 4) {
    for (int i = 1; i < _pipeCount; i++) {
      _pipes[i - 1] = _pipes[i];
      _sizedT[i - 1] = false;   // slot now shows a different pipe — re-size once
      _sizedB[i - 1] = false;
    }
    _pipeCount--;
  }

  _pipeTimer += kTickMs;
  if (_pipeTimer >= _pipeInterval) {
    _pipeTimer = 0;
    _spawnPipe();
  }

  // clouds drift
  for (int i = 0; i < kMaxClouds; i++) {
    lv_obj_t* cl = _clouds[i];
    int cx = lv_obj_get_x(cl) - 1;
    if (cx < -50) cx = _arenaW + 10;
    lv_obj_set_x(cl, cx);
  }

  if (_collision()) {
    if (Uni.Speaker) Uni.Speaker->playLose();
    _showResult();
  }
}

void GameFlappyScreen::_sync()
{
  if (_state != ST_PLAY) return;
  lv_obj_set_y(_bird, (int16_t)_birdY);

  for (int i = 0; i < kMaxPipes; i++) {
    if (i >= _pipeCount) {
      lv_obj_add_flag(_pipeT[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_pipeTC[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_pipeB[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_pipeBC[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    // Sizes are set once per life of a pipe (gapY never changes) — resizing
    // every frame forced a layout pass + large invalidations each tick and
    // the game ground to a stutter once the pipe slots filled up.
    int gapTop = _pipes[i].gapY - _gapH / 2;
    int gapBot = _pipes[i].gapY + _gapH / 2;
    int x = _pipes[i].x;

    lv_obj_clear_flag(_pipeT[i], LV_OBJ_FLAG_HIDDEN);
    if (!_sizedT[i]) {
      lv_obj_set_size(_pipeT[i], kPipeW, gapTop - kCapH);
      _sizedT[i] = true;
    }
    lv_obj_set_pos(_pipeT[i], x, 0);
    lv_obj_clear_flag(_pipeTC[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(_pipeTC[i], x - (kCapW - kPipeW) / 2, gapTop - kCapH);

    lv_obj_clear_flag(_pipeB[i], LV_OBJ_FLAG_HIDDEN);
    if (!_sizedB[i]) {
      lv_obj_set_size(_pipeB[i], kPipeW, (_arenaH - kGroundH) - gapBot - kCapH);
      _sizedB[i] = true;
    }
    lv_obj_set_pos(_pipeB[i], x, gapBot + kCapH);
    lv_obj_clear_flag(_pipeBC[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(_pipeBC[i], x - (kCapW - kPipeW) / 2, gapBot);
  }
}

void GameFlappyScreen::_menuFocus()
{
  if (_state != ST_MENU || !_overlay) return;
  for (int i = 0; i < 3; i++) gamekit::focusRow(_ovRows[i], i == _menuIdx);
}

// ── frame loop ──────────────────────────────────────────────────────────────

void GameFlappyScreen::onUpdate()
{
  _menuFocus();

  auto dir = _nav.poll();

  if (dir != INavigation::DIR_NONE) {
    if (_state == ST_MENU) {
      if      (dir == INavigation::DIR_UP)   _menuIdx = (_menuIdx + 2) % 3;
      else if (dir == INavigation::DIR_DOWN) _menuIdx = (_menuIdx + 1) % 3;
      else if (dir == INavigation::DIR_BACK) { Screen.goBack(); return; }
      else if (dir == INavigation::DIR_PRESS) {
        if      (_menuIdx == 0) _startPlay();
        else if (_menuIdx == 1) _showScores();
        else                  { Screen.goBack(); return; }
      }
    } else if (_state == ST_PLAY) {
      if (dir == INavigation::DIR_PRESS || dir == INavigation::DIR_UP) _flap();
      else if (dir == INavigation::DIR_BACK) _showMenu();
    } else if (_state == ST_RESULT) {
      if      (dir == INavigation::DIR_PRESS) _startPlay();
      else if (dir == INavigation::DIR_BACK) _showMenu();
    } else if (_state == ST_SCORES) {
      _showMenu();
    }
  }

  // physics — fixed tick with catch-up (bounded so a slow frame can't teleport)
  if (_state == ST_PLAY) {
    uint32_t now = millis();
    int steps = 0;
    while (now - _lastTickMs >= (uint32_t)kTickMs && steps < 4) {
      if (_state != ST_PLAY) { _lastTickMs = now; break; }
      _lastTickMs += kTickMs;
      _tick();
      steps++;
    }
    if (steps == 4) _lastTickMs = now;
    _sync();
  }
}

void GameFlappyScreen::onRender()
{
  // Nothing per-frame here — the LVGL shell calls render() once at init;
  // live updates happen in onUpdate() (see _sync).
}

#else  // ── legacy TFT build (unchanged) ──────────────────────────────────────

// ── Original legacy TFT implementation (from the repo, pre-LVGL port) ──

#include "GameFlappyScreen.h"
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "screens/game/GameMenuScreen.h"
#include "ui/actions/ShowStatusAction.h"

static constexpr float kGravity   = 0.35f;
static constexpr float kFlapForce = -3.2f;
static constexpr uint8_t kGroundH = 6;
static constexpr const char* kHsFile = "/unigeek/games/flappy_hs.txt";

// ── Lifecycle ───────────────────────────────────────────────────────────────

void GameFlappyScreen::onInit()
{
  _loadHighScores();
  render();
}

void GameFlappyScreen::onUpdate()
{
  bool hasNav = Uni.Nav->wasPressed();
  INavigation::Direction dir = hasNav ? Uni.Nav->readDirection() : INavigation::DIR_NONE;

  if (_state == STATE_PLAY) {
    // Input before frame gate for lowest jump latency
#ifdef DEVICE_HAS_TOUCH_NAV
    {
      bool touching = Uni.Nav->isPressed() &&
                      Uni.Nav->currentDirection() != INavigation::DIR_BACK;
      uint32_t now  = millis();
      if (touching && !_prevTouching) {
        _flap();
        _touchRepeatAt = now + kTouchFirstRepeatMs;
      } else if (touching && now >= _touchRepeatAt) {
        _flap();
        _touchRepeatAt = now + kTouchRepeatMs;
      }
      _prevTouching = touching;
      if (hasNav && dir == INavigation::DIR_BACK) { _state = STATE_MENU; render(); return; }
    }
#else
    if (hasNav) {
      if (dir == INavigation::DIR_PRESS || dir == INavigation::DIR_UP) _flap();
      else if (dir == INavigation::DIR_BACK) { _state = STATE_MENU; render(); return; }
    }
#ifdef DEVICE_HAS_KEYBOARD
    while (Uni.Keyboard->available()) {
      char c = Uni.Keyboard->getKey();
      if (c == ' ' || c == '\n') { _flap(); break; }
    }
#endif
#endif
    uint32_t now = millis();
    if (now - _lastFrameMs >= 33) {
      _lastFrameMs = now;
      _updateGame();
      if (_state == STATE_PLAY) render();
    }
    return;
  }

  if (!hasNav) return;

  if (_state == STATE_MENU) {
    switch (dir) {
      case INavigation::DIR_UP:   _menuIdx = (_menuIdx - 1 + kMenuItems) % kMenuItems; render(); break;
      case INavigation::DIR_DOWN: _menuIdx = (_menuIdx + 1) % kMenuItems;              render(); break;
      case INavigation::DIR_PRESS:
        if (_menuIdx == 0)      _initGame();
        else if (_menuIdx == 1) { _state = STATE_HIGH_SCORES; render(); }
        else                    Screen.goBack();
        break;
      case INavigation::DIR_BACK: Screen.goBack(); break;
      default: break;
    }
  } else if (_state == STATE_HIGH_SCORES) {
    if (dir != INavigation::DIR_NONE) { _state = STATE_MENU; render(); }
  } else if (_state == STATE_RESULT) {
    if (dir != INavigation::DIR_NONE) { _state = STATE_MENU; _menuIdx = 0; render(); }
  }
}

void GameFlappyScreen::onRender()
{
#ifdef USE_LVGL_SHELL
  if (!_scene.ensure(Uni.Lcd, bodyW(), bodyH())) return;
  _spr = &_scene.sprite();
#endif
  if      (_state == STATE_MENU)         _renderMenu();
  else if (_state == STATE_PLAY)         _renderPlay();
  else if (_state == STATE_HIGH_SCORES)  _renderHighScores();
  else                                   _renderResult();
#ifdef USE_LVGL_SHELL
  _scene.flush();
#endif
}

// ── Game Logic ──────────────────────────────────────────────────────────────

void GameFlappyScreen::_initGame()
{
  uint16_t h = bodyH();

  // Scale gap and speed to screen size
  _gapH         = h / 3;
  _pipeSpeed    = max(1, (int)(bodyW() / 40));
  _pipeInterval = max(800, 1800 - bodyW() * 4);

  _birdY     = h / 2.0f;
  _velocity  = 0;
  _score     = 0;
  _pipeCount = 0;
  _pipeTimer = 0;
  _lastFrameMs = millis();
  _state     = STATE_PLAY;

  _prevBirdY = -1;

#ifdef DEVICE_HAS_TOUCH_NAV
  _prevTouching  = false;
  _touchRepeatAt = 0;
#endif

  int n = Achievement.inc("flappy_first_play");
  if (n == 1) Achievement.unlock("flappy_first_play");

  // Spawn first pipe ahead
  _spawnPipe();
  render();
}

void GameFlappyScreen::_flap()
{
  _velocity = kFlapForce;
}

void GameFlappyScreen::_spawnPipe()
{
  if (_pipeCount >= kMaxPipes) return;

  uint16_t h     = bodyH() - kGroundH;
  uint8_t  minGY = _gapH / 2 + 4;
  uint8_t  maxGY = h - _gapH / 2 - 4;
  uint8_t  gapY  = minGY + random(maxGY - minGY + 1);

  int16_t spawnX = (int16_t)bodyW();
  _pipes[_pipeCount++] = { spawnX, spawnX, gapY, false };
}

void GameFlappyScreen::_updateGame()
{
  // Bird physics
  _velocity += kGravity;
  _birdY    += _velocity;

  // Pipe movement
  for (uint8_t i = 0; i < _pipeCount; i++) {
    _pipes[i].x -= _pipeSpeed;

    // Score when bird passes pipe center
    int birdX = 16;
    if (!_pipes[i].scored && _pipes[i].x + kPipeW < birdX) {
      _pipes[i].scored = true;
      _score++;
      if (Uni.Speaker && !Uni.Speaker->isPlaying()) Uni.Speaker->playRandomTone(0, 50);
      if (_score == 10)  Achievement.unlock("flappy_score_10");
      if (_score == 25)  Achievement.unlock("flappy_score_25");
      if (_score == 50)  Achievement.unlock("flappy_score_50");
      if (_score == 100) Achievement.unlock("flappy_score_100");
    }
  }

  // Remove off-screen pipes
  while (_pipeCount > 0 && _pipes[0].x < -kPipeW) {
    for (uint8_t i = 1; i < _pipeCount; i++)
      _pipes[i - 1] = _pipes[i];
    _pipeCount--;
  }

  // Spawn new pipes
  _pipeTimer += 33;
  if (_pipeTimer >= _pipeInterval) {
    _pipeTimer = 0;
    _spawnPipe();
  }

  // Collision
  if (_checkCollision()) {
    _lastScore = _score;
    if (_score > _bestScore) _bestScore = _score;
    _isNewHigh = false;
    _saveHighScore();
    if (Uni.Speaker) Uni.Speaker->playLose();
    ShowStatusAction::show("Game Over!", 1000);
    _state     = STATE_RESULT;
    render();
  }
}

bool GameFlappyScreen::_checkCollision()
{
  uint16_t h = bodyH() - kGroundH;
  int birdX = 16;
  int by    = (int)_birdY;

  // Floor / ceiling
  if (by <= 0 || by + kBirdH >= h) return true;

  // Pipes
  for (uint8_t i = 0; i < _pipeCount; i++) {
    int px = _pipes[i].x;
    // Check horizontal overlap
    if (birdX + kBirdW <= px || birdX >= px + kPipeW) continue;
    // Check if inside gap
    int gapTop = _pipes[i].gapY - _gapH / 2;
    int gapBot = _pipes[i].gapY + _gapH / 2;
    if (by < gapTop || by + kBirdH > gapBot) return true;
  }

  return false;
}

// ── Render ──────────────────────────────────────────────────────────────────

void GameFlappyScreen::_renderMenu()
{
#ifdef USE_LVGL_SHELL
  auto& lcd = *_spr;   // game renders into the canvas sprite
#else
  auto& lcd = Uni.Lcd;
#endif
  bool firstTime = (_prevState != STATE_MENU);
  if (firstTime) {
    lcd.fillRect(_bx(), _by(), bodyW(), bodyH(), TFT_BLACK);
    _prevState   = STATE_MENU;
    _lastMenuIdx = -1;
  }

  static constexpr const char* items[kMenuItems] = {"Play", "High Scores", "Exit"};
  lcd.setTextSize(2);
  const int lineH  = lcd.fontHeight() + 6;
  const int startY = (bodyH() - kMenuItems * lineH) / 2 - 8;

  for (int i = 0; i < kMenuItems; i++) {
    bool selNow  = (i == _menuIdx);
    bool selPrev = (i == _lastMenuIdx);
    if (!firstTime && selNow == selPrev) continue;

    Sprite sp(&lcd);
    sp.createSprite(bodyW(), lineH);
    sp.fillSprite(TFT_BLACK);
    sp.setTextDatum(MC_DATUM);
    sp.setTextSize(2);
    sp.setTextColor(selNow ? Config.getThemeColor() : TFT_WHITE, TFT_BLACK);
    sp.drawString(items[i], bodyW() / 2, lineH / 2);
    sp.pushSprite(_bx(), _by() + startY + i * lineH);
    sp.deleteSprite();
  }

  _lastMenuIdx = _menuIdx;

  if (firstTime) {
    lcd.setTextSize(1);
    lcd.setTextDatum(BC_DATUM);
    char buf[24];
    if (_lastScore > 0 && _bestScore > 0) {
      lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Last: %d", _lastScore);
      lcd.drawString(buf, _bx() + bodyW() / 2, _by() + bodyH() - 12);
      lcd.setTextColor(_lastScore == _bestScore ? TFT_YELLOW : TFT_DARKGREY, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Best: %d", _bestScore);
      lcd.drawString(buf, _bx() + bodyW() / 2, _by() + bodyH() - 2);
    } else if (_bestScore > 0) {
      lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
      snprintf(buf, sizeof(buf), "Best: %d", _bestScore);
      lcd.drawString(buf, _bx() + bodyW() / 2, _by() + bodyH() - 2);
    }
  }
}

void GameFlappyScreen::_renderPlay()
{
#ifdef USE_LVGL_SHELL
  auto& lcd = *_spr;   // game renders into the canvas sprite
#else
  auto& lcd = Uni.Lcd;
#endif
  int   bx     = _bx(), by0 = _by();
  int   w      = (int)bodyW(), h = (int)bodyH();
  int   birdX  = 16, birdY = (int)_birdY;
  int   groundTop = h - kGroundH;

  // One-time init: clear screen + draw static ground
  if (_prevState != STATE_PLAY) {
    lcd.fillRect(bx, by0, w, h, TFT_BLACK);
    lcd.fillRect(bx, by0 + groundTop, w, kGroundH, TFT_BROWN);
    for (int gx = 0; gx < w; gx += 8)
      lcd.fillRect(bx + gx, by0 + groundTop, 4, 2, TFT_OLIVE);
    _prevBirdY = -1;
    _fpsCount  = 0;
    _fps       = 0;
    _fpsTimer  = millis();
    _prevState = STATE_PLAY;
  }

  // FPS counter
  _fpsCount++;
  uint32_t now = millis();
  if (now - _fpsTimer >= 1000) {
    _fps = _fpsCount; _fpsCount = 0; _fpsTimer = now;
  }

  // 1. Erase old bird (body + beak extent)
  if (_prevBirdY >= 0)
    lcd.fillRect(bx + birdX - 1, by0 + _prevBirdY, kBirdW + 5, kBirdH, TFT_BLACK);

  // 2. Per-pipe: erase only the vacated trailing right strip, then draw
  //    prevX is stored in each Pipe struct — survives index shift on pipe removal
  for (uint8_t i = 0; i < _pipeCount; i++) {
    int px     = _pipes[i].x;
    int oldX   = _pipes[i].prevX;
    int gapTop = _pipes[i].gapY - _gapH / 2;
    int gapBot = _pipes[i].gapY + _gapH / 2;

    // Erase the strip vacated by the pipe this frame.
    // If the cap will be fully off-screen next frame, erase all the way to x=0
    // now — the pipe will be removed by _updateGame() before the next render
    // would have a chance to do it.
    if (px != oldX) {
      int oldCapR = min(oldX + kPipeW + 1, w);
      int newCapR = (px + kPipeW + 1 <= (int)_pipeSpeed)
                      ? 0
                      : max(0, min(px + kPipeW + 1, w));
      int eraseW  = oldCapR - newCapR;
      if (newCapR < w && eraseW > 0)
        lcd.fillRect(bx + newCapR, by0, eraseW, groundTop, TFT_BLACK);
    }

    // Draw pipe with coordinates clamped to body bounds
    if (px + kPipeW > 0 && px < w) {
      int bL = max(0, px),      bR = min(px + kPipeW,     w), bw = bR - bL;
      int cL = max(0, px - 1),  cR = min(px + kPipeW + 1, w), cw = cR - cL;

      if (bw > 0 && gapTop > 0)
        lcd.fillRect(bx + bL, by0, bw, gapTop, TFT_DARKGREEN);
      if (cw > 0 && gapTop > 3)
        lcd.fillRect(bx + cL, by0 + gapTop - 3, cw, 3, TFT_GREEN);

      if (bw > 0) {
        int botH = groundTop - gapBot;
        if (botH > 0) lcd.fillRect(bx + bL, by0 + gapBot, bw, botH, TFT_DARKGREEN);
      }
      if (cw > 0 && gapBot < groundTop)
        lcd.fillRect(bx + cL, by0 + gapBot, cw, min(3, groundTop - gapBot), TFT_GREEN);
    }

    _pipes[i].prevX = (int16_t)px;
  }

  // 3. Draw bird
  lcd.fillRect(bx + birdX,              by0 + birdY,     kBirdW, kBirdH, TFT_YELLOW);
  lcd.fillRect(bx + birdX + kBirdW - 3, by0 + birdY + 1, 2,      2,      TFT_BLACK);
  lcd.fillRect(bx + birdX + kBirdW,     by0 + birdY + 3, 3,      2,      TFT_ORANGE);
  _prevBirdY = (int16_t)birdY;

  // 4. Score + FPS — always redraw on top to cover pipe overdraw
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", _score);
  lcd.setTextDatum(TC_DATUM);
  lcd.setTextSize(2);
  lcd.setTextColor(TFT_WHITE);
  lcd.drawString(buf, bx + w / 2, by0 + 2);

  snprintf(buf, sizeof(buf), "%ufps", _fps);
  lcd.setTextDatum(TR_DATUM);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_DARKGREY);
  lcd.drawString(buf, bx + w - 2, by0 + 2);
}

void GameFlappyScreen::_renderResult()
{
  if (_prevState == STATE_RESULT) return;
  _prevState = STATE_RESULT;

#ifdef USE_LVGL_SHELL
  auto& lcd = *_spr;   // game renders into the canvas sprite
#else
  auto& lcd = Uni.Lcd;
#endif
  lcd.fillRect(_bx(), _by(), bodyW(), bodyH(), TFT_BLACK);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextSize(2);

  lcd.setTextColor(TFT_RED, TFT_BLACK);
  lcd.drawString("Game Over!", _bx() + bodyW() / 2, _by() + bodyH() / 2 - 24);

  char buf[24];
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  snprintf(buf, sizeof(buf), "Score: %d", _score);
  lcd.drawString(buf, _bx() + bodyW() / 2, _by() + bodyH() / 2);

  if (_isNewHigh) {
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.drawString("NEW HIGH!", _bx() + bodyW() / 2, _by() + bodyH() / 2 + 14);
  } else {
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "Best: %d", _bestScore);
    lcd.drawString(buf, _bx() + bodyW() / 2, _by() + bodyH() / 2 + 14);
  }

  lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  lcd.setTextDatum(BC_DATUM);
  lcd.drawString("Press to continue", _bx() + bodyW() / 2, _by() + bodyH());
}

// ── High Score Storage ───────────────────────────────────────────────────────

void GameFlappyScreen::_loadHighScores()
{
  memset(_topScores, 0, sizeof(_topScores));
  if (!Uni.Storage || !Uni.Storage->exists(kHsFile)) return;
  String data = Uni.Storage->readFile(kHsFile);
  int pos = 0;
  for (uint8_t i = 0; i < kTopN; i++) {
    int nl = data.indexOf('\n', pos);
    if (nl < 0) break;
    _topScores[i] = data.substring(pos, nl).toInt();
    pos = nl + 1;
  }
  _bestScore = _topScores[0];
}

void GameFlappyScreen::_saveHighScore()
{
  // Check if score belongs in top N
  if (_score <= 0) return;
  bool entered = false;
  for (uint8_t i = 0; i < kTopN; i++) {
    if (_score > _topScores[i]) {
      // Shift down
      for (uint8_t j = kTopN - 1; j > i; j--)
        _topScores[j] = _topScores[j - 1];
      _topScores[i] = _score;
      _isNewHigh    = true;
      entered       = true;
      break;
    }
  }
  if (!entered) return;

  _bestScore = _topScores[0];

  if (!Uni.Storage) return;
  String data;
  for (uint8_t i = 0; i < kTopN; i++) {
    data += String(_topScores[i]);
    data += '\n';
  }
  Uni.Storage->makeDir("/unigeek/games");
  Uni.Storage->writeFile(kHsFile, data.c_str());
}

void GameFlappyScreen::_renderHighScores()
{
  if (_prevState == STATE_HIGH_SCORES) return;
  _prevState = STATE_HIGH_SCORES;

#ifdef USE_LVGL_SHELL
  auto& lcd = *_spr;   // game renders into the canvas sprite
#else
  auto& lcd = Uni.Lcd;
#endif
  lcd.fillRect(_bx(), _by(), bodyW(), bodyH(), TFT_BLACK);

  lcd.setTextSize(1);
  lcd.setTextDatum(TC_DATUM);
  lcd.setTextColor(Config.getThemeColor(), TFT_BLACK);
  lcd.drawString("Top Scores", _bx() + bodyW() / 2, _by() + 2);

  const int lineH  = 14;
  const int startY = 18;
  char buf[16];
  for (uint8_t i = 0; i < kTopN; i++) {
    uint16_t col = (i == 0) ? TFT_YELLOW : TFT_WHITE;
    if (_topScores[i] == 0) col = TFT_DARKGREY;
    lcd.setTextColor(col, TFT_BLACK);
    lcd.setTextDatum(TL_DATUM);
    snprintf(buf, sizeof(buf), "#%u", i + 1);
    lcd.drawString(buf, _bx() + 8, _by() + startY + i * lineH);
    lcd.setTextDatum(TR_DATUM);
    if (_topScores[i] > 0)
      snprintf(buf, sizeof(buf), "%d", _topScores[i]);
    else
      snprintf(buf, sizeof(buf), "--");
    lcd.drawString(buf, _bx() + bodyW() - 8, _by() + startY + i * lineH);
  }

  lcd.setTextSize(1);
  lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  lcd.setTextDatum(BC_DATUM);
  lcd.drawString("Press to go back", _bx() + bodyW() / 2, _by() + bodyH() - 1);
}
#endif  // USE_LVGL_SHELL
