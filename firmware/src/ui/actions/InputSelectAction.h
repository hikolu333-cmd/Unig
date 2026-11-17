//
// Created by L Shaf on 2026-02-23.
//

#pragma once

#include "core/Device.h"
#include "utils/uart/UartFileManager.h"
#include "core/ConfigManager.h"
#include "ui/lvgl/LvglUi.h"   // LvglUi::pump() is a no-op without USE_LVGL_SHELL

class InputSelectAction
{
public:
  struct Option {
    const char* label;
    const char* value;
    const char* sublabel;
    int16_t     rssi;
    bool        hasRssi;
  };

  static const char* popup(const char* title, const Option* options, uint8_t count, const char* defaultValue = nullptr) {
    InputSelectAction action(title, options, count, defaultValue);
    const char* result = action._run();
    Uni.lastActiveMs = millis();
    return result;
  }

private:
  static constexpr int PAD = 4;

  const char*   _title;
  const Option* _options;
  uint8_t       _count;
  uint8_t       _totalCount;
  int           _selectedIdx   = 0;
  bool          _done          = false;
  const char*   _result        = nullptr;

  explicit InputSelectAction(const char* title, const Option* options, uint8_t count, const char* defaultValue)
  : _title(title), _options(options),
    _count(count), _totalCount(count + 1)
  {
    if (defaultValue != nullptr) {
      for (int i = 0; i < _count; i++) {
        if (strcmp(_options[i].value, defaultValue) == 0) {
          _selectedIdx = i;
          break;
        }
      }
    }
  }

  const char* _labelAt(int idx) {
    if (idx < _count) return _options[idx].label;
    return "Cancel";
  }

  const char* _valueAt(int idx) {
    if (idx < _count) return _options[idx].value;
    return nullptr;
  }

  static uint16_t _rssiColor(int16_t rssi) {
    if (rssi >= -60) return TFT_GREEN;
    if (rssi >= -75) return TFT_YELLOW;
    return TFT_RED;
  }

#ifdef USE_LVGL_SHELL
  // ── LVGL mode: modal list on the top layer ───────────────────────────────
  static constexpr int ROW_H   = 24;
  static constexpr int TITLE_H = 16;

  lv_obj_t* _card   = nullptr;
  lv_obj_t* _list   = nullptr;
  lv_obj_t* _titleLbl = nullptr;
  lv_obj_t* _counter   = nullptr;
  int       _rows      = 0;

  bool _isCancel(int idx) { return idx == _count; }

  const char* _run() {
    _build();

    while (!_done) {
      Uni.update();
      UartFM.poll(); // read remote input so nav works in this dialog
      if (Mirror.dirty()) Mirror.pump(); // flush only when this overlay redrew
      LvglUi::pump();

      if (Uni.Nav->wasPressed()) {
        switch (Uni.Nav->readDirection()) {
          case INavigation::DIR_UP:
            _focusRow(_selectedIdx, false);
            _selectedIdx = (_selectedIdx - 1 + _totalCount) % _totalCount;
            _focusRow(_selectedIdx, true);
            _updateCounter();
            break;

          case INavigation::DIR_DOWN:
            _focusRow(_selectedIdx, false);
            _selectedIdx = (_selectedIdx + 1) % _totalCount;
            _focusRow(_selectedIdx, true);
            _updateCounter();
            break;

          case INavigation::DIR_PRESS:
            _result = _valueAt(_selectedIdx);
            _done   = true;
            break;

          case INavigation::DIR_BACK:
            _result = nullptr;
            _done   = true;
            break;

          default: break;
        }
      }
      delay(10);
    }

    lv_obj_del(_card);
    _card = nullptr;
    LvglUi::repaintScreen();
    return _result;
  }

  int _visibleCount() {
    int available = Uni.Lcd.height() - (PAD + TITLE_H + PAD) - PAD * 3;
    int fitCount  = available / (ROW_H + PAD);
    return min((int)_totalCount, fitCount);
  }

  void _build() {
    LvglUi::ensure();
    int visible = _visibleCount();
    int listH   = visible * (ROW_H + PAD);
    int h = PAD + TITLE_H + PAD + listH + PAD;
    int w = Uni.Lcd.width() - (PAD * 2 + 8);

    _card = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(_card, PAD + 4, (Uni.Lcd.height() - h) / 2);
    lv_obj_set_size(_card, w, h);
    lv_obj_set_style_bg_color(_card, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_card, lv_color_white(), 0);
    lv_obj_set_style_border_width(_card, 1, 0);
    lv_obj_set_style_radius(_card, 4, 0);
    lv_obj_set_style_pad_all(_card, 0, 0);
    lv_obj_clear_flag(_card, LV_OBJ_FLAG_SCROLLABLE);

    _titleLbl = lv_label_create(_card);
    lv_obj_set_pos(_titleLbl, PAD, PAD);
    lv_obj_set_width(_titleLbl, w - 50 - PAD);
    lv_label_set_long_mode(_titleLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(_titleLbl, LvglUi::c565(TFT_YELLOW), 0);
    lv_label_set_text(_titleLbl, _title);

    _counter = lv_label_create(_card);
    lv_obj_align(_counter, LV_ALIGN_TOP_RIGHT, -PAD, PAD);
    lv_obj_set_style_text_color(_counter, LvglUi::c565(TFT_DARKGREY), 0);
    lv_label_set_text(_counter, "");

    _list = lv_obj_create(_card);
    lv_obj_set_pos(_list, PAD, PAD + TITLE_H + PAD);
    lv_obj_set_size(_list, w - PAD * 2, listH);
    lv_obj_set_style_bg_opa(_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_list, 0, 0);
    lv_obj_set_style_pad_all(_list, 0, 0);
    lv_obj_set_style_pad_row(_list, PAD, 0);
    lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(_list, LV_SCROLLBAR_MODE_OFF);

    _rows = _totalCount;
    for (int i = 0; i < _totalCount; i++) {
      lv_obj_t* row = lv_btn_create(_list);
      lv_obj_set_size(row, lv_pct(100), ROW_H);
      LvglUi::styleRow(row);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      const Option* opt = (i < _count) ? &_options[i] : nullptr;
      bool cancel = (i == _count);

      lv_obj_t* lbl = lv_label_create(row);
      lv_obj_set_pos(lbl, 6, 4);
      lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
      lv_obj_set_width(lbl, lv_pct(opt && opt->sublabel ? 55 : 88));
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
      if (opt && opt->hasRssi)
        lv_obj_set_style_text_color(lbl, LvglUi::c565(_rssiColor(opt->rssi)), 0);
      else if (cancel)
        lv_obj_set_style_text_color(lbl, LvglUi::c565(TFT_RED), 0);
      lv_label_set_text(lbl, _labelAt(i));

      if (opt && opt->sublabel) {
        lv_obj_t* sub = lv_label_create(row);
        lv_obj_align(sub, LV_ALIGN_RIGHT_MID, -6, 0);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        lv_obj_set_width(sub, lv_pct(40));
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(sub, LvglUi::c565(TFT_DARKGREY), 0);
        lv_label_set_text(sub, opt->sublabel);
      }
    }
    _focusRow(_selectedIdx, true);
    _updateCounter();
    lv_timer_handler();
  }

  void _focusRow(int idx, bool focused) {
    if (!_list || idx < 0 || idx >= _rows) return;
    lv_obj_t* row = lv_obj_get_child(_list, idx);
    if (!row) return;
    if (focused) {
      lv_obj_add_state(row, LV_STATE_FOCUSED);
      lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    } else {
      lv_obj_clear_state(row, LV_STATE_FOCUSED);
    }
  }

  void _updateCounter() {
    if (!_counter) return;
    int visible = _visibleCount();
    if (_totalCount > visible) {
      char buf[8];
      int displayIdx = min((int)_selectedIdx + 1, (int)_count);
      snprintf(buf, sizeof(buf), "%d/%d", displayIdx, _count);
      lv_label_set_text(_counter, buf);
    } else {
      lv_label_set_text(_counter, "");
    }
  }

#else
  // ── Legacy TFT mode ──────────────────────────────────────────────────────
  static constexpr int ITEM_H  = 18;
  static constexpr int TITLE_H = 12;

  int           _scrollOffset  = 0;

  bool _isCancel(int idx) {
    return idx == _count;
  }

  int _visibleCount() {
    auto& lcd     = Uni.Lcd;
    int available = lcd.height()
                  - (PAD + TITLE_H + PAD)
                  - PAD * 3;
    int fitCount  = available / (ITEM_H + PAD);
    return min((int)_totalCount, fitCount);
  }

  int _overlayW() { return Uni.Lcd.width() - (PAD * 2 + 8); }
  int _overlayH() {
    return PAD + TITLE_H + PAD
         + (_visibleCount() * (ITEM_H + PAD))
         + PAD;
  }
  int _overlayX() { return PAD + 4; }
  int _overlayY() { return (Uni.Lcd.height() - _overlayH()) / 2; }
  int _listY()    { return PAD + TITLE_H + PAD; }

  bool _scrollToSelected() {
    int visible = _visibleCount();
    int prev    = _scrollOffset;
    if (_selectedIdx < _scrollOffset) {
      _scrollOffset = _selectedIdx;
    } else if (_selectedIdx >= _scrollOffset + visible) {
      _scrollOffset = _selectedIdx - visible + 1;
    }
    return prev != _scrollOffset;
  }

  const char* _run() {
    _drawChrome();
    _drawCounter();
    int visible = _visibleCount();
    for (int i = 0; i < visible && (_scrollOffset + i) < _totalCount; i++)
      _drawRow(_scrollOffset + i);

    int lastSelected = _selectedIdx;

    while (!_done) {
      Uni.update();
      UartFM.poll(); // read remote input so nav works in this dialog
      if (Mirror.dirty()) Mirror.pump(); // flush only when this overlay redrew

      if (Uni.Nav->wasPressed()) {
        switch (Uni.Nav->readDirection()) {
          case INavigation::DIR_UP:
            _selectedIdx = (_selectedIdx - 1 + _totalCount) % _totalCount;
            _onSelectionChanged(lastSelected);
            lastSelected = _selectedIdx;
            break;

          case INavigation::DIR_DOWN:
            _selectedIdx = (_selectedIdx + 1) % _totalCount;
            _onSelectionChanged(lastSelected);
            lastSelected = _selectedIdx;
            break;

          case INavigation::DIR_PRESS:
            _result = _valueAt(_selectedIdx);
            _done   = true;
            break;

          case INavigation::DIR_BACK:
            _result = nullptr;
            _done   = true;
            break;

          default: break;
        }
      }
      delay(10);
    }

    Uni.Lcd.fillRect(_overlayX(), _overlayY(), _overlayW(), _overlayH(), TFT_BLACK);
    return _result;
  }

  void _onSelectionChanged(int lastSelected) {
    bool scrolled = _scrollToSelected();
    _drawCounter();
    if (scrolled) {
      int visible = _visibleCount();
      for (int i = 0; i < visible && (_scrollOffset + i) < _totalCount; i++)
        _drawRow(_scrollOffset + i);
    } else {
      if (lastSelected != _selectedIdx) _drawRow(lastSelected);
      _drawRow(_selectedIdx);
    }
  }

  void _drawChrome() {
    auto& lcd = Uni.Lcd;
    int w = _overlayW();
    int h = _overlayH();
    int x = _overlayX();
    int y = _overlayY();

    lcd.fillRect(x, y, w, h, TFT_BLACK);
    lcd.drawRoundRect(x, y, w, h, 4, TFT_WHITE);

    lcd.setTextColor(TFT_YELLOW);
    lcd.setTextSize(1);
    lcd.setTextDatum(TL_DATUM);
    lcd.setCursor(x + PAD, y + PAD);
    lcd.print(_title);

  }

  void _drawCounter() {
    auto& lcd = Uni.Lcd;
    int w = _overlayW();
    int x = _overlayX();
    int y = _overlayY();
    int visible = _visibleCount();

    // Clear counter area whether or not we need it.
    int cw = 40;
    lcd.fillRect(x + w - PAD - cw, y + PAD, cw, TITLE_H, TFT_BLACK);

    if (_totalCount > visible) {
      char buf[8];
      int displayIdx = min((int)_selectedIdx + 1, (int)_count);
      snprintf(buf, sizeof(buf), "%d/%d", displayIdx, _count);
      lcd.setTextColor(TFT_DARKGREY);
      lcd.setTextDatum(TR_DATUM);
      lcd.drawString(buf, x + w - PAD, y + PAD);
    }
  }

  void _drawRow(int idx) {
    if (idx < _scrollOffset) return;
    int visible = _visibleCount();
    int slot    = idx - _scrollOffset;
    if (slot >= visible) return;
    if (idx >= _totalCount) return;

    auto&    lcd        = Uni.Lcd;
    uint16_t themeColor = Config.getThemeColor();
    int w      = _overlayW();
    int x      = _overlayX();
    int y      = _overlayY();
    int innerW = w - PAD * 2;

    int itemY    = _listY() + slot * (ITEM_H + PAD);
    bool isSel   = (idx == _selectedIdx);
    bool isCancel = _isCancel(idx);

    Sprite sp(&lcd);
    sp.createSprite(innerW, ITEM_H);
    sp.fillSprite(TFT_BLACK);
    sp.setTextSize(1);

    const Option* opt = idx < _count ? &_options[idx] : nullptr;
    if (isSel) {
      uint16_t boxColor = isCancel ? TFT_RED : themeColor;
      sp.fillRoundRect(0, 0, innerW, ITEM_H, 3, boxColor);
      sp.drawRoundRect(0, 0, innerW, ITEM_H, 3, TFT_WHITE);
    }

    uint16_t labelColor = isCancel ? (isSel ? TFT_WHITE : TFT_RED)
                          : (opt && opt->hasRssi ? _rssiColor(opt->rssi)
                                                : (isSel ? TFT_WHITE : TFT_LIGHTGREY));
    sp.setTextColor(labelColor, isSel ? (isCancel ? TFT_RED : themeColor) : TFT_BLACK);
    sp.setTextDatum(ML_DATUM);

    int labelAvailW = innerW - 8;
    if (opt && opt->sublabel) {
      sp.setTextColor(TFT_DARKGREY, isSel ? themeColor : TFT_BLACK);
      sp.setTextDatum(MR_DATUM);
      sp.drawString(opt->sublabel, innerW - 4, ITEM_H / 2);
      labelAvailW = innerW - 12 - sp.textWidth(opt->sublabel);
      sp.setTextColor(labelColor, isSel ? themeColor : TFT_BLACK);
      sp.setTextDatum(ML_DATUM);
    }

    String label = _labelAt(idx);
    while (label.length() > 1 && sp.textWidth(label) > labelAvailW)
      label.remove(label.length() - 1);
    sp.drawString(label, 4, ITEM_H / 2);

    sp.pushSprite(x + PAD, y + itemY);
    sp.deleteSprite();
  }
#endif
};
