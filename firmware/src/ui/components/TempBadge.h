#pragma once
//
// TempBadge — chip-temperature corner badge for attack/spam screens.
// The sensor is sampled at most every 5 s; the badge itself redraws on every
// call so panel-drawing attack loops can't erase it.
//
#include <Arduino.h>
#include <math.h>
#include "core/Device.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class TempBadge {
public:
#ifdef USE_LVGL_SHELL
  // LVGL label pinned to the body's top-right — for screens that render
  // through LVGL (labels, lists, LogView). The cached pointer is validated
  // against parent teardown (lv_obj_is_valid), like TagPrompt.
  static void drawLv() {
    lv_obj_t*& lbl = _lvLbl();
    if (lbl && !lv_obj_is_valid(lbl)) lbl = nullptr;
    if (!lbl) {
      lbl = lv_label_create(LvglUi::viewParent());
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
      lv_obj_align(lbl, LV_ALIGN_TOP_RIGHT, -4, 0);
      lv_label_set_text(lbl, "--C");
    }
    const char* txt; uint16_t col;
    _sample(txt, col);
    if (strcmp(lv_label_get_text(lbl), txt) != 0) {
      lv_label_set_text(lbl, txt);
      lv_obj_set_style_text_color(lbl, LvglUi::c565(col), 0);
    }
  }

  // Drop the badge when the attack stops — otherwise it lingers over menus.
  static void hideLv() {
    lv_obj_t*& lbl = _lvLbl();
    if (lbl) {
      if (lv_obj_is_valid(lbl)) lv_obj_del(lbl);
      lbl = nullptr;
    }
  }
#else
  // No LVGL shell on this board — the drawLv/hideLv API stays, as no-ops,
  // so shared screens can call it unguarded.
  static void drawLv() {}
  static void hideLv() {}
#endif

  // Small TFT sprite in the panel's top-right corner — for screens that
  // draw their attack state directly on the panel (works under both shells).
  static void drawTft() {
    auto& lcd = Uni.Lcd;
    const char* txt; uint16_t col;
    _sample(txt, col);
    const int w = 44, h = 14;
    Sprite sp(&lcd);
    if (!sp.createSprite(w, h)) return;
    sp.fillSprite(TFT_BLACK);
    sp.setTextSize(1);
    sp.setTextDatum(TR_DATUM);
    sp.setTextColor(col, TFT_BLACK);
    sp.drawString(txt, w, h / 2);
    sp.pushSprite(lcd.width() - w - 2, 2);
    sp.deleteSprite();
  }

private:
  // Sample the ESP32 internal sensor, at most every 5 s; text + colour are
  // cached between samples. ASCII-only text: the TFT base font has no degree
  // glyph, and LVGL montserrat keeps both shells consistent.
  static void _sample(const char*& txt, uint16_t& col) {
    static uint32_t last = 0;
    static char     buf[8] = "";
    static uint16_t c      = TFT_DARKGREY;
    if (buf[0] == '\0' || millis() - last >= 5000) {
      last = millis();
      const float t = temperatureRead();
      if (isnan(t)) {
        snprintf(buf, sizeof(buf), "--C");
        c = TFT_DARKGREY;
      } else {
        snprintf(buf, sizeof(buf), "%dC", (int)(t + 0.5f));
        c = (t >= 80.0f) ? TFT_RED
          : (t >= 65.0f) ? TFT_ORANGE
                         : TFT_DARKGREY;
      }
    }
    txt = buf; col = c;
  }

#ifdef USE_LVGL_SHELL
  static lv_obj_t*& _lvLbl() { static lv_obj_t* l = nullptr; return l; }
#endif
};
