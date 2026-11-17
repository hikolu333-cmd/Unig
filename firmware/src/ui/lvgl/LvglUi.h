//
// LvglUi — shared LVGL session for the USE_LVGL_SHELL builds.
//
// Both the shell (home + main menu) and every ListScreen-based page render
// through one lv_disp (flushed via Uni.Lcd.pushImage, which also feeds the
// web screen mirror). LvglUi::ensure() brings LVGL up exactly once; page
// roots are full-screen opaque lv_obj's stacked on lv_scr_act() — a pushed
// page visually covers whatever is below, and deleting the root (screen
// destructor) invalidates the area so the page underneath repaints.
//
#pragma once

#ifdef USE_LVGL_SHELL

#include <lvgl.h>

namespace LvglUi
{  // One-time LVGL bring-up: lv_init, draw buffer + display driver, default
  // theme with the configured accent colour. Safe to call repeatedly.
  void ensure();

  // Full-screen opaque page root with a title header. Deleting it makes
  // LVGL repaint whatever was underneath (previous page / shell). The
  // created root also becomes the "current root" — see currentRoot().
  lv_obj_t* pageRoot(const char* title);

  // The newest pageRoot() that is still alive (lv_scr_act() fallback).
  // Views parent their widgets here so a page teardown removes them all.
  lv_obj_t* currentRoot();

  // Called by the page owner just before deleting a pageRoot() — keeps
  // currentRoot() from dangling.
  void forgetRoot(lv_obj_t* root);

  // One lv_timer_handler() pass: renders invalidated widgets to the panel.
  // Called by BaseScreen::update() and by every blocking popup loop.
  void pump();

  // Full-screen opaque container on lv_layer_top() — the LVGL twin of the
  // legacy blocking popups (ShowStatus / Input*). Delete it when done, then
  // repaintScreen() so the page underneath comes back.
  lv_obj_t* overlay();

  // Invalidate the whole active screen — used after an overlay/page is
  // deleted so LVGL repaints what was underneath.
  void repaintScreen();

  // Scrolling flex-column list that fills a pageRoot below its header.
  // Scrollbar is hidden (it read as coloured side stripes on the panel);
  // scrolling itself stays enabled for long lists.
  lv_obj_t* makeList(lv_obj_t* root);

  // Opaque body surface on the current page root, used as the parent for
  // view widgets (ScrollListView, LogView, ...). Sits above the page's list
  // in z-order, so views painted over the list behave like the legacy
  // sprite overlays; viewParentHide() gives the body back to the list.
  lv_obj_t* viewParent();

  // Hide the body surface (list re-took the body after setItems()).
  void viewParentHide();

  // Vertical offset between absolute body coordinates (bodyY()) and the
  // viewParent() origin — pages without a title have bodyY()==0.
  int viewOffsetY();

  // Shared row look (normal + LV_STATE_FOCUSED) for list rows and menu
  // buttons: dark rounded row, accent background + white border when focused.
  void styleRow(lv_obj_t* row);

  // RGB565 (TFT_eSPI convention) → lv_color_t, for views that keep the
  // legacy colour constants (TFT_RED etc.) as their palette.
  lv_color_t c565(uint16_t rgb565);

  // Grid key look for the keyboard popups (InputText/InputNumber/InputBip):
  // same visual language as styleRow, sized for absolute grid placement.
  void styleKey(lv_obj_t* key);
}

#else

// Non-LVGL boards: the shared view/popup code calls LvglUi::pump() in its
// event loops — a no-op there so call sites need no #ifdef of their own.
namespace LvglUi
{
  inline void pump() {}
}

#endif // USE_LVGL_SHELL
