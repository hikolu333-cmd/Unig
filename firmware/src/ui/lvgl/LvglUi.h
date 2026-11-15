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
{
  // One-time LVGL bring-up: lv_init, draw buffer + display driver, default
  // theme with the configured accent colour. Safe to call repeatedly.
  void ensure();

  // Full-screen opaque page root with a title header. Deleting it makes
  // LVGL repaint whatever was underneath (previous page / shell).
  lv_obj_t* pageRoot(const char* title);

  // Scrolling flex-column list that fills a pageRoot below its header.
  // Scrollbar is hidden (it read as coloured side stripes on the panel);
  // scrolling itself stays enabled for long lists.
  lv_obj_t* makeList(lv_obj_t* root);

  // Shared row look (normal + LV_STATE_FOCUSED) for list rows and menu
  // buttons: dark rounded row, accent background + white border when focused.
  void styleRow(lv_obj_t* row);
}

#endif // USE_LVGL_SHELL
