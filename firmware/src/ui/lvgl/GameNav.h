#pragma once

//
// GameNav — shared stick input for native-LVGL games (USE_LVGL_SHELL).
//
// The board nav reports a tap only on RELEASE (wasPressed/readDirection),
// one event per flick. Menus are fine with that, but games need held-repeat
// (cycle a letter, sweep a value). This helper merges both sources:
//
//   - a released flick fires once (works with the web-remote inject too);
//   - holding a direction past kFirstMs auto-repeats every kRepeatMs;
//   - the release tap is swallowed if the repeater already fired for that
//     hold, so one flick == exactly one event, a hold == a stream.
//
// PRESS repeats too (at the slower rate) — convenient for Number Guess.
//

#ifdef USE_LVGL_SHELL

#include <lvgl.h>
#include "core/Device.h"
#include "core/ConfigManager.h"
#include "ui/lvgl/LvglUi.h"

class GameNav
{
public:
  using Dir = INavigation::Direction;

  // Call once per game loop. Returns Dir::DIR_NONE or exactly one direction.
  Dir poll()
  {
    uint32_t now = millis();

    // Released tap (stick flick, remote inject, short SW press).
    if (Uni.Nav->wasPressed()) {
      Dir d = Uni.Nav->readDirection();
      _lastDir = Dir::DIR_NONE;
      _armed   = false;
      if (_repFired && d == _repDir) {           // repeater already serviced this hold
        _repFired = false;
        return Dir::DIR_NONE;
      }
      _repFired = false;
      return d;
    }

    // Held-repeat from the live direction (stick held past the deadzone).
    if (Uni.Nav->isPressed()) {
      Dir d = Uni.Nav->currentDirection();
      if (d == Dir::DIR_NONE) return Dir::DIR_NONE;
      if (d != _lastDir) {                        // fresh press edge → immediate
        _lastDir = d;
        _t0 = now;
        _armed = false;
        _repFired = true;                          // this hold was serviced → swallow release tap
        _repDir = d;
        return d;
      }
      uint32_t first = (d == Dir::DIR_PRESS) ? kFirstPressMs : kFirstMs;
      uint32_t rep   = (d == Dir::DIR_PRESS) ? kRepeatPressMs : kRepeatMs;
      if (!_armed && now - _t0 >= first) {
        _armed = true;
        _tRep  = now;
        _repFired = true;
        _repDir = d;
        return d;
      }
      if (_armed && now - _tRep >= rep) {
        _tRep  = now;
        _repFired = true;
        _repDir = d;
        return d;
      }
      return Dir::DIR_NONE;
    }

    _lastDir = Dir::DIR_NONE;
    _armed   = false;
    _repFired = false;
    return Dir::DIR_NONE;
  }

private:
  static constexpr uint32_t kFirstMs        = 380;   // hold before repeats start
  static constexpr uint32_t kRepeatMs        = 130;   // repeat rate for directions
  static constexpr uint32_t kFirstPressMs    = 500;   // PRESS repeats slower
  static constexpr uint32_t kRepeatPressMs    = 250;

  Dir      _lastDir  = Dir::DIR_NONE;
  Dir      _repDir   = Dir::DIR_NONE;
  uint32_t _t0       = 0;
  uint32_t _tRep     = 0;
  bool     _armed    = false;
  bool     _repFired = false;
};

// ── Shared visual kit for the games ──────────────────────────────────────────
// One look everywhere — dark theme: near-black sky, dark navy panels and
// cells, white text, accent highlights (nothing bright behind the letters).
// All helpers parent to the given container and return the created widget.

namespace gamekit {

inline lv_obj_t* arena(lv_obj_t* parent, uint32_t bg = 0x0f1626)
{
  // onInit() runs before the first LVGL layout pass: a just-created parent
  // (viewBody) still reports 0x0. Force the layout now so the arena gets the
  // real arena size — otherwise the 0x0 arena clips every child (black games).
  lv_obj_update_layout(parent);
  lv_obj_t* a = lv_obj_create(parent);
  lv_obj_set_pos(a, 0, 0);
  lv_obj_set_size(a, lv_obj_get_width(parent), lv_obj_get_height(parent));
  lv_obj_set_style_bg_color(a, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(a, 0, 0);
  lv_obj_set_style_pad_all(a, 0, 0);
  lv_obj_set_style_radius(a, 0, 0);
  lv_obj_clear_flag(a, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(a, LV_SCROLLBAR_MODE_OFF);
  return a;
}

// Centered modal panel: dark surface, accent border, rounded.
inline lv_obj_t* panel(lv_obj_t* parent, int w, int h, uint32_t bg = 0x1a2438)
{
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_center(p);
  lv_obj_set_size(p, w, h);
  lv_obj_set_style_bg_color(p, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(p, LvglUi::c565(Config.getThemeColor()), 0);
  lv_obj_set_style_border_width(p, 2, 0);
  lv_obj_set_style_radius(p, 10, 0);
  lv_obj_set_style_pad_all(p, 10, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_OFF);
  return p;
}

inline lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color,
                       lv_align_t align, int x, int y, const char* txt)
{
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_align(l, align, x, y);
  lv_label_set_text(l, txt);
  return l;
}

// A rounded "key cap" cell (Wordle/Decoder/Memory). Owns its text.
inline lv_obj_t* cell(lv_obj_t* parent, int x, int y, int w, int h,
                       uint32_t bg, int radius = 6)
{
  lv_obj_t* c = lv_label_create(parent);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_text_font(c, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_align(c, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(c, lv_color_white(), 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(0x0d1321), 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_radius(c, radius, 0);
  lv_label_set_text(c, "");
  return c;
}

inline void setCellText(lv_obj_t* c, char ch)
{
  char buf[2] = { ch, 0 };
  lv_label_set_text(c, buf);
}

inline void paintCell(lv_obj_t* c, uint32_t bg, uint32_t fg = 0xffffff,
                      uint32_t border = 0x060a12)
{
  lv_obj_set_style_bg_color(c, lv_color_hex(bg), 0);
  lv_obj_set_style_text_color(c, lv_color_hex(fg), 0);
  lv_obj_set_style_border_color(c, lv_color_hex(border), 0);
}

// Focused / unfocused menu row styling. row is the label widget.
inline void focusRow(lv_obj_t* row, bool sel)
{
  lv_obj_set_style_text_color(row, sel ? lv_color_black() : lv_color_white(), 0);
  lv_obj_set_style_bg_color(row,
    sel ? LvglUi::c565(Config.getThemeColor()) : lv_color_hex(0x1a2438), 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(row, 16, 0);
  lv_obj_set_style_pad_ver(row, 5, 0);
  lv_obj_set_style_radius(row, 6, 0);
}

}  // namespace gamekit

#endif  // USE_LVGL_SHELL
