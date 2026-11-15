//
// LvglShell — LVGL-powered home screen + main menu (USE_LVGL_SHELL boards).
//
// Lives inside the stock ScreenManager as a normal IScreen: launching a legacy
// screen (Wifi attacks, BLE, IR, games, …) works through the same
// Screen.push()/goBack() as the old menu — while a legacy screen is current,
// this screen (and therefore lv_timer_handler) simply isn't updated, so LVGL
// is naturally paused. On goBack, onRestore() invalidates the whole LVGL
// screen and the shell redraws.
//
// Navigation is the stock INavigation (stick + PC remote): on the home page
// PRESS/BACK opens the menu, in the menu UP/DOWN move focus, PRESS launches.
//
#pragma once

#include "core/IScreen.h"

class LvglShellScreen : public IScreen
{
public:
  void init() override;
  void update() override;
  void render() override;
  void onRestore() override;

private:
  // one-time LVGL bring-up (display driver + UI tree)
  void _ensureUi();
  void _showMenu();   // home -> menu
  void _showHome();   // menu -> home
  void _selectMenuItem(int index);
};
