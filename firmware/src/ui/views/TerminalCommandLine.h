#pragma once

#include "core/Device.h"
#include "core/IKeyboard.h"
#include "ui/lvgl/LvglUi.h"   // LvglUi::pump() is a no-op without USE_LVGL_SHELL

class TerminalCommandLine
{
public:
  enum Action : uint8_t {
    ACTION_NONE,
    ACTION_CHANGED,
    ACTION_SUBMIT,
    ACTION_EXIT,
    ACTION_SCROLL_UP,
    ACTION_SCROLL_DOWN,
    ACTION_SCROLL_LEFT,
    ACTION_SCROLL_RIGHT,
  };

  static constexpr int MAX_LEN = 256;

  const String& text() const { return _text; }
  void clear() { _text = ""; }

  Action consume(IKeyboard* keyboard) {
    if (!keyboard || !keyboard->available()) return ACTION_NONE;

    uint8_t modifiers = keyboard->modifiers();
    char c = keyboard->getKey();

    // The Cardputer family uses Fn + ; . , / as the four arrow keys.
    if (modifiers & IKeyboard::MOD_FN) {
      if (c == ';') return ACTION_SCROLL_UP;
      if (c == '.') return ACTION_SCROLL_DOWN;
      if (c == ',') return ACTION_SCROLL_LEFT;
      if (c == '/') return ACTION_SCROLL_RIGHT;
    }

    if (c == '\b' || c == 0x7f) {
      if (_text.length() > 0) {
        _text.remove(_text.length() - 1);
        return ACTION_CHANGED;
      }
      return ACTION_EXIT;
    }

    if (c == '\n' || c == '\r') return ACTION_SUBMIT;

    if (c >= 0x20 && c <= 0x7e && (int)_text.length() < MAX_LEN) {
      _text += c;
      return ACTION_CHANGED;
    }

    return ACTION_NONE;
  }

#ifdef USE_LVGL_SHELL
  void render(int x, int y, int w, int h, bool remoteClosed, bool keyboardDevice) {
    if (!_cont) {
      LvglUi::ensure();
      _cont = lv_obj_create(LvglUi::viewParent());
      lv_obj_set_style_bg_color(_cont, lv_color_black(), 0);
      lv_obj_set_style_bg_opa(_cont, LV_OPA_COVER, 0);
      lv_obj_set_style_border_color(_cont, LvglUi::c565(TFT_DARKGREY), 0);
      lv_obj_set_style_border_width(_cont, 1, 0);
      lv_obj_set_style_radius(_cont, 3, 0);
      lv_obj_set_style_pad_all(_cont, 2, 0);
      lv_obj_clear_flag(_cont, LV_OBJ_FLAG_SCROLLABLE);

      _label = lv_label_create(_cont);
      lv_obj_set_width(_label, lv_pct(100));
      lv_label_set_long_mode(_label, LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_font(_label, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(_label, lv_color_white(), 0);
    }
    lv_obj_set_pos(_cont, x, y - LvglUi::viewOffsetY());
    lv_obj_set_size(_cont, w, h);

    String line;
    if (remoteClosed) {
      line = "BACK: Exit";
    } else if (keyboardDevice) {
      line = "> " + _text + "_";
    } else {
      line = "> PRESS: Type";
    }

    lv_obj_set_style_text_color(_label, LvglUi::c565(remoteClosed ? TFT_DARKGREY : TFT_WHITE), 0);
    lv_label_set_text(_label, line.c_str());
  }

private:
  String   _text;
  lv_obj_t* _cont  = nullptr;
  lv_obj_t* _label = nullptr;

#else // ── legacy TFT mode ───────────────────────────────────────────────────

  void render(int x, int y, int w, int h, bool remoteClosed, bool keyboardDevice) {
    auto& lcd = Uni.Lcd;
    lcd.fillRect(x, y, w, h, TFT_BLACK);
    lcd.setTextFont(1);
    lcd.setTextSize(1);
    lcd.setTextDatum(TL_DATUM);
    lcd.setTextColor(remoteClosed ? TFT_DARKGREY : TFT_WHITE, TFT_BLACK);

    String line;
    if (remoteClosed) {
      line = "BACK: Exit";
    } else if (keyboardDevice) {
      line = "> " + _text + "_";
    } else {
      line = "> PRESS: Type";
    }

    int maxChars = max(4, (w - 8) / 6);
    if ((int)line.length() > maxChars) {
      if (!remoteClosed && keyboardDevice && maxChars > 3) {
        int keep = maxChars - 3;
        line = ">.." + line.substring(line.length() - keep);
      } else {
        line = line.substring(0, maxChars);
      }
    }

    lcd.drawString(line.c_str(), x + 4, y + max(0, (h - 8) / 2));
  }

private:
  String _text;
#endif
};
