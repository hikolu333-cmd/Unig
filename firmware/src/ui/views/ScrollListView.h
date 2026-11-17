//
// Created by L Shaf on 2026-03-01.
//

#pragma once
#include "core/Device.h"
#include "core/INavigation.h"
#include "core/ConfigManager.h"
#include <cstring>
#include "ui/lvgl/LvglUi.h"   // LvglUi::pump() is a no-op without USE_LVGL_SHELL
#include "ui/components/TagPrompt.h"

// Label/value table with wheel scrolling. Dual-mode:
//  - legacy: sprite rows pushed to the panel;
//  - USE_LVGL_SHELL: a flex-column of rows inside an LVGL container on the
//    current page root; scrolling drives the LVGL scroll position.
class ScrollListView
{
public:
  struct Row {
    const char* label;
    String      value;
  };

#ifdef USE_LVGL_SHELL
  static constexpr int ROW_H = 18;   // montserrat_14 line + breathing room

  void setRows(Row* rows, uint16_t count) {
    _rows  = rows;
    _count = count;
    // clamp offset so the list stays scrolled as far as possible
    int fullyVisible = (_h > 0) ? _h / ROW_H : 1;
    if (fullyVisible < 1) fullyVisible = 1;
    int maxOffset = (_count > fullyVisible) ? _count - fullyVisible : 0;
    if (_offset > maxOffset) _offset = maxOffset;
    _dirty = true;
  }

  void resetScroll() { _offset = 0; _scrollLV(); }

  void render(int x, int y, int w, int h) {
    _x = x; _y = y; _w = w; _h = h;
    if (!_cont) _build();
    // A live TagPrompt layer yields the body back once real rows are drawn.
    TagPrompt::clear();
    // viewParent() sits at the page's body top — map absolute body coords.
    lv_obj_set_pos(_cont, x, y - LvglUi::viewOffsetY());
    lv_obj_set_size(_cont, w, h);
    lv_obj_clear_flag(_cont, LV_OBJ_FLAG_HIDDEN);
    if (_dirty) { _rebuild(); _dirty = false; }
    _scrollLV();
  }

  // Hide the widget tree without destroying it — screens that flip between
  // an overview grid and a detail list need the list gone while the grid
  // is up. render() unhides again. Legacy mode clears with fillRect anyway.
  void hide() {
    if (_cont) lv_obj_add_flag(_cont, LV_OBJ_FLAG_HIDDEN);
  }

  // returns true if direction was consumed (scrolled)
  bool onNav(INavigation::Direction dir) {
    // Boundary uses fully-visible rows only: if the last row is partial,
    // the user can still scroll one more step to bring it fully into view.
    int fullyVisible = _h / ROW_H;
    if (fullyVisible < 1) fullyVisible = 1;
    int maxOffset = (_count > fullyVisible) ? _count - fullyVisible : 0;
    if (dir == INavigation::DIR_UP && _offset > 0) {
      _offset--;
      _scrollLV();
      return true;
    }
    if (dir == INavigation::DIR_DOWN && _offset < maxOffset) {
      _offset++;
      _scrollLV();
      return true;
    }
    if (dir == INavigation::DIR_LEFT && _offset > 0) {
      _offset -= fullyVisible;
      if (_offset < 0) _offset = 0;
      _scrollLV();
      return true;
    }
    if (dir == INavigation::DIR_RIGHT && _offset < maxOffset) {
      _offset += fullyVisible;
      if (_offset > maxOffset) _offset = maxOffset;
      _scrollLV();
      return true;
    }
    return false;
  }

private:
  Row*     _rows   = nullptr;
  uint16_t _count  = 0;
  int      _offset = 0;
  int      _x = 0, _y = 0, _w = 0, _h = 0;
  bool     _dirty  = true;
  lv_obj_t* _cont  = nullptr;

  void _build() {
    LvglUi::ensure();
    _cont = lv_obj_create(LvglUi::viewParent());
    lv_obj_set_style_bg_opa(_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_cont, 0, 0);
    lv_obj_set_style_pad_all(_cont, 0, 0);
    lv_obj_set_style_pad_row(_cont, 0, 0);
    lv_obj_set_flex_flow(_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
  }

  void _rebuild() {
    lv_obj_clean(_cont);
    for (uint16_t i = 0; i < _count; i++) {
      const char* label = _rows[i].label;
      const bool actionHint = label &&
          (strncmp(label, "[Press]", 7) == 0 || strncmp(label, "[Hold]", 6) == 0);

      lv_obj_t* row = lv_obj_create(_cont);
      lv_obj_set_size(row, lv_pct(100), ROW_H);
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(row, 0, 0);
      lv_obj_set_style_pad_all(row, 0, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t* l = lv_label_create(row);
      lv_obj_set_pos(l, 2, 2);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
      lv_obj_set_width(l, lv_pct(55));
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, LvglUi::c565(actionHint ? TFT_YELLOW : TFT_DARKGREY), 0);
      lv_label_set_text(l, label ? label : "");

      lv_obj_t* v = lv_label_create(row);
      lv_obj_align(v, LV_ALIGN_RIGHT_MID, -2, 0);
      lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
      lv_obj_set_width(v, lv_pct(42));
      lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(v, LvglUi::c565(TFT_WHITE), 0);
      lv_label_set_text(v, _rows[i].value.c_str());
    }
  }

  void _scrollLV() {
    if (_cont) lv_obj_scroll_to_y(_cont, _offset * ROW_H, LV_ANIM_OFF);
  }

#else // ── legacy TFT mode ───────────────────────────────────────────────────

  void setRows(Row* rows, uint16_t count) {
    _rows  = rows;
    _count = count;
    // clamp offset so the list stays scrolled as far as possible
    int fullyVisible = (_h > 0) ? _h / ROW_H : 1;
    if (fullyVisible < 1) fullyVisible = 1;
    int maxOffset = (_count > fullyVisible) ? _count - fullyVisible : 0;
    if (_offset > maxOffset) _offset = maxOffset;
  }

  void resetScroll() { _offset = 0; }

  void render(int x, int y, int w, int h) {
    _x = x; _y = y; _w = w; _h = h;
    _draw();
  }

  // returns true if direction was consumed (scrolled)
  bool onNav(INavigation::Direction dir) {
    // Boundary uses fully-visible rows only: if the last row is partial,
    // the user can still scroll one more step to bring it fully into view.
    int fullyVisible = _h / ROW_H;
    if (fullyVisible < 1) fullyVisible = 1;
    int maxOffset = (_count > fullyVisible) ? _count - fullyVisible : 0;
    if (dir == INavigation::DIR_UP && _offset > 0) {
      _offset--;
      _draw();
      return true;
    }
    if (dir == INavigation::DIR_DOWN && _offset < maxOffset) {
      _offset++;
      _draw();
      return true;
    }
    if (dir == INavigation::DIR_LEFT && _offset > 0) {
      _offset -= fullyVisible;
      if (_offset < 0) _offset = 0;
      _draw();
      return true;
    }
    if (dir == INavigation::DIR_RIGHT && _offset < maxOffset) {
      _offset += fullyVisible;
      if (_offset > maxOffset) _offset = maxOffset;
      _draw();
      return true;
    }
    return false;
  }

private:
  static constexpr int ROW_H    = 14;
  static constexpr int SCROLL_W = 3;

  Row*    _rows   = nullptr;
  uint16_t _count = 0;
  int     _offset = 0;
  int     _x = 0, _y = 0, _w = 0, _h = 0;

  void _draw() {
    auto& lcd = Uni.Lcd;

    int full    = _h / ROW_H;
    int extra   = (_h % ROW_H >= 5) ? 1 : 0;
    int visible = full + extra;
    int textW   = _w - SCROLL_W - 4;

    lcd.setTextSize(1);

    int usedH = 0;
    for (int i = 0; i < visible; i++) {
      int idx = i + _offset;
      if (idx >= _count) break;

      int rowY = _y + i * ROW_H;
      int rowH = ROW_H;
      int remaining = (_y + _h) - rowY;
      if (remaining <= 0) break;
      if (rowH > remaining) rowH = remaining;

      Sprite sp(&lcd);
      sp.createSprite(_w - SCROLL_W, rowH);
      sp.fillSprite(TFT_BLACK);
      sp.setTextSize(1);

      const char* label = _rows[idx].label;
      const bool actionHint = label &&
          (strncmp(label, "[Press]", 7) == 0 || strncmp(label, "[Hold]", 6) == 0);
      sp.setTextColor(actionHint ? TFT_YELLOW : TFT_DARKGREY);
      sp.setTextDatum(TL_DATUM);
      sp.drawString(label ? label : "", 2, 3);

      sp.setTextColor(actionHint ? TFT_YELLOW : TFT_WHITE);
      sp.setTextDatum(TR_DATUM);
      sp.drawString(_rows[idx].value.c_str(), textW, 3);

      sp.pushSprite(_x, rowY);
      sp.deleteSprite();
      usedH += rowH;
    }

    // Clear unused rows below last rendered row.
    if (usedH < _h)
      lcd.fillRect(_x, _y + usedH, _w - SCROLL_W, _h - usedH, TFT_BLACK);

    // Scrollbar — drawn directly; only if content overflows.
    lcd.fillRect(_x + _w - SCROLL_W, _y, SCROLL_W, _h, 0x2104);
    int fullyVisible = _h / ROW_H;
    if (fullyVisible < 1) fullyVisible = 1;
    if (_count > fullyVisible) {
      int sbH = max(4, _h * fullyVisible / _count);
      if (sbH > _h) sbH = _h;
      int maxOffset = _count - fullyVisible;
      int sbY = (_h - sbH) * _offset / max(1, maxOffset);
      if (sbY < 0)              sbY = 0;
      if (sbY > _h - sbH)       sbY = _h - sbH;
      lcd.fillRect(_x + _w - SCROLL_W, _y + sbY, SCROLL_W, sbH, Config.getThemeColor());
    }
  }
#endif
};
