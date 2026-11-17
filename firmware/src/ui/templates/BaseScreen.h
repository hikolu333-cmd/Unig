#pragma once

#include "core/IScreen.h"
#include "core/AchievementManager.h"
#include "ui/components/Header.h"
#include "ui/components/StatusBar.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class BaseScreen : public IScreen
{
public:
#ifdef USE_LVGL_SHELL
  // ── LVGL mode ────────────────────────────────────────────────────────────
  // The page root is an opaque full-screen lv_obj built before onInit(), so
  // views created during onInit can parent to LvglUi::currentRoot(). No
  // legacy fillScreen(TFT_BLACK): LVGL paints the new page straight over the
  // previous one (a black wipe added a jarring full-black frame on the slow
  // SPI bus). Deleting the root (destructor) invalidates the covered area so
  // the page underneath repaints.
  ~BaseScreen() override { _lvTeardown(); }

  void init() override {
    _lvEnsureRoot();
    onInit();
    render();
  }

  // The page is a widget tree — no per-loop sprite/chrome drawing. update()
  // only feeds events and pumps the LVGL timer so invalidated widgets flush.
  void update() override {
    onUpdate();
    if (!Uni.lcdOff && _lvRoot) lv_timer_handler();
  }

  void render() override {
    if (Uni.lcdOff) return;
    onRender();
  }

  // Returning from a legacy (non-LVGL) leaf screen: the panel was drawn over
  // directly — invalidate our page so LVGL repaints it.
  void onRestore() override {
    if (_lvRoot) lv_obj_invalidate(_lvRoot);
  }

  // Body area helpers — public so views (e.g. BrowseFileView) can query them.
  // LVGL layout: full width below the page title (pageRoot draws the header).
  uint16_t bodyX() { return 0; }
  uint16_t bodyY() { return title() ? 34 : 0; }
  uint16_t bodyW() { return Uni.Lcd.width() - 4; }
  uint16_t bodyH() { return Uni.Lcd.height() - bodyY() - 4; }

#else
  // ── Legacy TFT mode ──────────────────────────────────────────────────────
  void init() override {
    Uni.Lcd.fillScreen(TFT_BLACK);
    onInit();
    render();
  }

  void update() override {
    onUpdate();
    Achievement.drawToastIfNeeded(bodyX(), bodyY(), bodyW(), bodyH());
    if (Uni.lcdOff) return;
    if (!isFullScreen() && millis() - _lastStatusUpdate > 1000) {
      StatusBar::refresh();
      _lastStatusUpdate = millis();
    }
  }

  void render() override {
    if (Uni.lcdOff) return;
    if (!isFullScreen()) _renderChrome();
    onRender();
  }

  // Body area helpers — public so views (e.g. BrowseFileView) can query them.
  uint16_t bodyX() { return StatusBar::WIDTH; }
  uint16_t bodyY() { return title() ? Header::HEIGHT : 0; }
  uint16_t bodyW() { return Uni.Lcd.width() - StatusBar::WIDTH - 4; }
  uint16_t bodyH() { return Uni.Lcd.height() - bodyY() - 4; }
#endif

protected:
  // ─── Subclass overrides ───────────────────────────────
  virtual const char* title() { return nullptr; }  // nullptr = no header
  virtual bool isFullScreen() { return false; }    // true = skip header + status bar

  virtual void onInit()   {}
  virtual void onUpdate() {}
  virtual void onRender() {}

#ifdef USE_LVGL_SHELL
  lv_obj_t* _lvRoot = nullptr;   // full-screen page root (owns every view widget)

  void _lvEnsureRoot() {
    if (!_lvRoot) _lvRoot = LvglUi::pageRoot(title());
  }

  void _lvTeardown() {
    if (_lvRoot) {
      LvglUi::forgetRoot(_lvRoot);
      lv_obj_del(_lvRoot);        // invalidates the covered area → repaint below
      _lvRoot = nullptr;
    }
  }
#else
private:
  Header   _header;
  uint32_t _lastStatusUpdate = 0;

  void _renderChrome() {
    _header.render(title());
    StatusBar::refresh();
  }
#endif
};
