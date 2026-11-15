#pragma once
#include "BaseScreen.h"
#include "core/ScreenManager.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

class ListScreen : public BaseScreen
{
public:
  // NOTE: no default member initializers here — this project builds with a
  // C++ standard where NSDMI disqualify aggregate-init, and dozens of call
  // sites rely on brace-init like `{"Label"}` / `{"Label", "Sub"}`, which
  // needs ListItem to stay a plain aggregate (trailing members then get
  // zero/false from aggregate-init, same effective default).
  struct ListItem
  {
    const char* label;
    const char* sublabel;
    int16_t     rssi;               // dBm, only meaningful when hasRssi
    bool        hasRssi;            // when true, label is tinted by RSSI strength
    bool        sublabelMarquee;    // selected row scrolls an overlong right-hand sublabel
  };

#ifdef USE_LVGL_SHELL
public:
  ~ListScreen() override { _lvTeardown(); }

  // LVGL mode: no legacy fillScreen(TFT_BLACK) here — the page root is an
  // opaque full-screen object, so LVGL paints the new page straight over the
  // previous one. The black wipe added a jarring full-black frame between
  // pages on the slow SPI bus.
  void init() override { onInit(); render(); }

  // LVGL mode: the page is a widget tree — no per-loop sprite/chrome
  // drawing. update() only feeds events + pumps LVGL timers; render()
  // (called on setItems / display wake) syncs the tree and paints.
  void render() override {
    if (Uni.lcdOff) return;
    onRender();
  }
  void update() override {
    onUpdate();
    if (!Uni.lcdOff && _lvRoot) lv_timer_handler();
  }
  // Returning from a legacy (non-LVGL) leaf screen: the panel was drawn over
  // directly — invalidate our page so LVGL repaints it.
  void onRestore() override {
    if (_lvRoot) lv_obj_invalidate(_lvRoot);
  }
#endif

  template <size_t N>
  void setItems(ListItem (&arr)[N])
  {
    _items            = arr;
    _count            = N;
    _selectedIndex    = 0;
    _scrollOffset     = 0;
    _partialTopActive = false;
    _scrollIfNeeded();
    render();
  }

  void setItems(ListItem* arr, uint8_t count)
  {
    _items         = arr;
    _count         = count;
    _selectedIndex = 0;
    _scrollOffset  = 0;
    _partialTopActive = false;
    _scrollIfNeeded();
    render();
  }

  void setItems(ListItem* arr, uint8_t count, uint8_t selectedIdx)
  {
    _items         = arr;
    _count         = count;
    uint8_t eff    = count;
    _selectedIndex = (eff > 0 && selectedIdx < eff) ? selectedIdx : 0;
    _scrollOffset  = 0;
    _partialTopActive = false;
    _scrollIfNeeded();
    render();
  }

  void onInit() override
  {
    render();
  }

  void onUpdate() override
  {
    if (Uni.Nav->wasPressed())
    {
      auto dir = Uni.Nav->readDirection();

      if (dir == INavigation::DIR_BACK)
      {
        onBack();
        return;
      }

      uint8_t eff = _effectiveCount();
      if (eff == 0) return;

      if (dir == INavigation::DIR_UP)
      {
        _selectedIndex = (_selectedIndex == 0) ? eff - 1 : _selectedIndex - 1;
#ifdef USE_LVGL_SHELL
        _lvApplyFocus();
#else
        _scrollIfNeeded();
        _resetMarquee();
        onRender();
#endif
        if (Uni.Speaker) Uni.Speaker->beep();
      }
      else if (dir == INavigation::DIR_DOWN)
      {
        _selectedIndex = (_selectedIndex >= eff - 1) ? 0 : _selectedIndex + 1;
#ifdef USE_LVGL_SHELL
        _lvApplyFocus();
#else
        _scrollIfNeeded();
        _resetMarquee();
        onRender();
#endif
        if (Uni.Speaker) Uni.Speaker->beep();
      }
      else if (dir == INavigation::DIR_LEFT)
      {
        uint8_t page = bodyH() / ITEM_H;
        _selectedIndex = (_selectedIndex >= page) ? _selectedIndex - page : 0;
#ifdef USE_LVGL_SHELL
        _lvApplyFocus();
#else
        _scrollIfNeeded();
        _resetMarquee();
        onRender();
#endif
        if (Uni.Speaker) Uni.Speaker->beep();
      }
      else if (dir == INavigation::DIR_RIGHT)
      {
        uint8_t page = bodyH() / ITEM_H;
        uint8_t last = eff - 1;
        _selectedIndex = (_selectedIndex + page <= last) ? _selectedIndex + page : last;
#ifdef USE_LVGL_SHELL
        _lvApplyFocus();
#else
        _scrollIfNeeded();
        _resetMarquee();
        onRender();
#endif
        if (Uni.Speaker) Uni.Speaker->beep();
      }
      else if (dir == INavigation::DIR_PRESS)
      {
        onItemSelected(_selectedIndex);
      }
      return;
    }

#ifndef USE_LVGL_SHELL
    _updateMarquee();
#endif
  }

  void onRender() override
  {
#ifdef USE_LVGL_SHELL
    _lvRender();
#else
    uint8_t eff          = _effectiveCount();
    uint8_t fullyVisible = bodyH() / ITEM_H;
    int16_t leftover     = (int16_t)bodyH() - fullyVisible * (int16_t)ITEM_H;
    bool    hasPartial   = leftover >= 5;
    int16_t listW        = bodyW() - 4;

    auto& lcd = Uni.Lcd;

    if (eff == 0) {
      lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
      return;
    }

    auto renderRow = [&](uint8_t idx, int16_t screenY, int16_t rowH, int16_t dy) {
      const ListItem* item     = &_items[idx];
      bool     selected = (idx == _selectedIndex);
      uint16_t bg       = selected ? Config.getThemeColor() : TFT_BLACK;
      uint16_t fg       = item->hasRssi ? _rssiColor(item->rssi)
                                         : (selected ? TFT_WHITE : TFT_LIGHTGREY);

      Sprite sprite(&lcd);
      sprite.createSprite(listW, rowH);
      sprite.fillSprite(TFT_BLACK);
      sprite.setTextDatum(TL_DATUM);

      if (selected)
        sprite.fillRoundRect(0, 2 + dy, listW, ITEM_H - 4, 3, bg);

      sprite.setTextColor(fg, bg);

      int16_t labelAvailW = listW - 12;
      if (_stackedSublabels && item->sublabel)
      {
        // Detail layout: field name above, value below. Short values remain
        // right-aligned; long selected values marquee within the full row.
        sprite.drawString(item->label, 6, 2 + dy);

        sprite.setTextColor(selected ? TFT_CYAN : TFT_DARKGREY, bg);
        int16_t subAvailW = listW - 12;
        if (selected && sprite.textWidth(item->sublabel) > subAvailW) {
          String subText = _marqueeWindow(sprite, item->sublabel, subAvailW);
          int16_t subX = listW - 6 - sprite.textWidth(subText);
          if (subX < 6) subX = 6;
          sprite.drawString(subText, subX, 11 + dy);
        } else {
          int16_t subX = listW - 6 - sprite.textWidth(item->sublabel);
          if (subX < 6) subX = 6;
          sprite.drawString(item->sublabel, subX, 11 + dy);
        }
      }
      else
      {
        if (item->sublabel)
        {
          int16_t subMaxW = item->sublabelMarquee ? (listW / 2) : (listW - 12);
          int16_t labelW = sprite.textWidth(item->label);
          int16_t subW = sprite.textWidth(item->sublabel);
          bool subOverflows = subW > subMaxW;
          bool preferSub = selected && _preferSublabel &&
                           (labelW + 4 + subW > (listW - 12));

          sprite.setTextColor(selected ? TFT_CYAN : TFT_DARKGREY, bg);

          String subText = item->sublabel;
          if (subOverflows) {
            if (selected) {
              subText = _marqueeWindow(sprite, item->sublabel, subMaxW);
            } else {
              int16_t n = subText.length();
              while (n > 1 && sprite.textWidth(subText.substring(0, n)) > subMaxW) n--;
              subText = subText.substring(0, n);
            }
          }

          int16_t subX = listW - 6 - sprite.textWidth(subText);
          if (subX < 6) subX = 6;
          sprite.drawString(subText, subX, (ITEM_H / 2) - 4 + dy);

          int16_t reservedSubW = subW > subMaxW ? subMaxW : subW;
          int16_t reservedSubX = listW - 6 - reservedSubW;
          if (reservedSubX < 6) reservedSubX = 6;

          // A preferred selected sublabel gets the row when label + sublabel
          // do not fit. A dedicated BSSID-style marquee remains on the right,
          // leaving the primary label static/truncated.
          labelAvailW = (selected && preferSub && !item->sublabelMarquee)
                          ? 0
                          : reservedSubX - 6 - 4;
          sprite.setTextColor(fg, bg);
        }

        if (labelAvailW > 0)
        {
          bool subMarqueeActive = selected && item->sublabel &&
                                  item->sublabelMarquee &&
                                  sprite.textWidth(item->sublabel) > (listW / 2);
          if (selected && !subMarqueeActive &&
              sprite.textWidth(item->label) > labelAvailW) {
            sprite.drawString(_marqueeWindow(sprite, item->label, labelAvailW),
                              6, (ITEM_H / 2) - 4 + dy);
          } else if (sprite.textWidth(item->label) > labelAvailW) {
            String labelText = item->label;
            int16_t n = labelText.length();
            while (n > 1 &&
                   sprite.textWidth(labelText.substring(0, n)) > labelAvailW) n--;
            sprite.drawString(labelText.substring(0, n),
                              6, (ITEM_H / 2) - 4 + dy);
          } else {
            sprite.drawString(item->label, 6, (ITEM_H / 2) - 4 + dy);
          }
        }
      }
      sprite.pushSprite(bodyX(), bodyY() + screenY);
      sprite.deleteSprite();
    };

    int16_t curY  = 0;
    int16_t usedH = 0;

    bool showPartialTop = hasPartial && _scrollOffset > 0 && _partialTopActive;

    // Scrolled down: show bottom `leftover` px of row above, top clipped.
    if (showPartialTop)
    {
      int16_t dy = -(int16_t)(ITEM_H - leftover);
      renderRow(_scrollOffset - 1, 0, leftover, dy);
      curY = leftover;
    }

    // Full rows.
    uint8_t rendered = 0;
    for (uint8_t i = 0; i < fullyVisible; i++)
    {
      uint8_t idx = i + _scrollOffset;
      if (idx >= eff) break;
      renderRow(idx, curY + i * ITEM_H, ITEM_H, 0);
      rendered++;
    }
    usedH = curY + rendered * ITEM_H;

    // Peek of next row at bottom (when no partial top is shown).
    if (hasPartial && !showPartialTop)
    {
      uint8_t idx = _scrollOffset + rendered;
      if (idx < eff)
      {
        renderRow(idx, usedH, leftover, 0);
        usedH += leftover;
      }
    }

    if (usedH < (int16_t)bodyH())
      lcd.fillRect(bodyX(), bodyY() + usedH, bodyW(), bodyH() - usedH, TFT_BLACK);

    {
      static constexpr uint8_t SB_W = 3;
      int16_t sbX = bodyX() + bodyW() - SB_W;
      int16_t sbY = bodyY();
      int16_t sbH = bodyH();
      lcd.fillRect(sbX, sbY, SB_W, sbH, 0x2104);
      if (eff <= fullyVisible) {
        lcd.fillRect(sbX, sbY, SB_W, sbH, Config.getThemeColor());
      } else {
        int16_t thumbH = sbH * (int16_t)fullyVisible / (int16_t)eff;
        if (thumbH < 8) thumbH = 8;
        int16_t thumbY = sbY + ((int16_t)_scrollOffset * (sbH - thumbH)) / (int16_t)(eff - fullyVisible);
        lcd.fillRect(sbX, thumbY, SB_W, thumbH, Config.getThemeColor());
      }
    }
#endif // !USE_LVGL_SHELL
  }

  virtual void onItemSelected(uint8_t index) = 0;
  virtual void onBack() { Screen.goBack(); }
protected:
  uint8_t _selectedIndex = 0;

  // Render sublabels on a second line, right-aligned. Intended for detail
  // views where values (banners, TXT records, URLs, etc.) may be long.
  // The selected long value uses the existing marquee so the full text can
  // be read without changing ListItem or increasing per-row storage.
  void setStackedSublabels(bool enabled)
  {
    _stackedSublabels = enabled;
    _resetMarquee();
  }

  // In compact result lists, prefer the selected sublabel when label +
  // sublabel cannot fit together. This is useful for rows such as
  // "IP / hostname", where the hostname is the value that needs inspection.
  void setPreferSublabel(bool enabled)
  {
    _preferSublabel = enabled;
    _resetMarquee();
  }

  // Update only the count after in-place array edits (SettingScreen pattern).
  // Clamps selection and adjusts scroll — does NOT call render(). Caller must.
  void setCount(uint8_t count)
  {
    _count = count;
    uint8_t eff = _effectiveCount();
    if (eff > 0 && _selectedIndex >= eff) _selectedIndex = eff - 1;
    _scrollIfNeeded();
    _marqueeOffset = 0;
  }

private:
  ListItem*     _items            = nullptr;
  uint8_t       _count            = 0;
  uint8_t       _scrollOffset     = 0;
  bool          _partialTopActive = false;
  bool          _stackedSublabels = false;
  bool          _preferSublabel = false;

  uint32_t      _marqueeTimer     = 0;
  int16_t       _marqueeOffset    = 0;

  static constexpr uint8_t ITEM_H = 22;

  static uint16_t _rssiColor(int16_t rssi)
  {
    if (rssi >= -60) return TFT_GREEN;
    if (rssi >= -75) return TFT_YELLOW;
    return TFT_RED;
  }

  void _resetMarquee()
  {
    _marqueeOffset = 0;
    _marqueeTimer  = millis();
  }

  // Redraws the whole list (small — a handful of rows, each a cheap sprite
  // blit) only while the selected row's label actually overflows its
  // available width. No-op for every other ListScreen subclass's rows.
  void _updateMarquee()
  {
    uint8_t eff = _effectiveCount();
    if (eff == 0 || _selectedIndex >= eff) return;

    const ListItem& item   = _items[_selectedIndex];
    int16_t         listW  = bodyW() - 4;
    int16_t         availW = listW - 12;
    const char* marqueeText = item.label;
    if (_stackedSublabels && item.sublabel) {
      // In detail layout the value gets the full row width and is the text
      // users most need to inspect completely.
      marqueeText = item.sublabel;
    } else if (item.sublabel) {
      int16_t subMaxW = item.sublabelMarquee ? (listW / 2) : (listW - 12);
      int16_t subW = Uni.Lcd.textWidth(item.sublabel);
      int16_t labelW = Uni.Lcd.textWidth(item.label);
      bool preferSub = _preferSublabel &&
                       (labelW + 4 + subW > (listW - 12));

      if (subW > subMaxW || preferSub) {
        marqueeText = item.sublabel;
        availW = subMaxW;
      } else {
        availW = (listW - 6 - subW) - 10;
      }
    }

    if (Uni.Lcd.textWidth(marqueeText) <= availW) {
      if (_marqueeOffset != 0) { _marqueeOffset = 0; onRender(); }
      return;
    }

    if (millis() - _marqueeTimer < 180) return;
    _marqueeTimer = millis();
    _marqueeOffset++;
    onRender();
  }

  // Returns the visible slice of `label` for the current marquee offset,
  // shrinking from the right until it fits `availW` pixels. The label loops
  // ("label     label     ...") so the ticker scrolls seamlessly.
  String _marqueeWindow(Sprite& sprite, const char* label, int16_t availW)
  {
    String base(label);
    base += "     ";
    int16_t loopLen = (int16_t)base.length();
    if (loopLen <= 0) return String(label);

    int16_t off = _marqueeOffset % loopLen;
    String  doubled = base + base;
    String  window  = doubled.substring(off);

    int16_t n = window.length();
    while (n > 1 && sprite.textWidth(window.substring(0, n)) > availW) n--;
    return window.substring(0, n);
  }

  uint8_t _effectiveCount()
  {
    return _count;
  }

#ifdef USE_LVGL_SHELL
  // ── LVGL page: title header + scrolling flex-column of row buttons ──────
private:
  lv_obj_t* _lvRoot        = nullptr;
  lv_obj_t* _lvList        = nullptr;
  int       _lvPrevSel     = -1;
  bool      _lvNeedRebuild = false;

  static lv_color_t _c565(uint16_t c) {
    return lv_color_make((uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
                         (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
                         (uint8_t)((c & 0x1F) * 255 / 31));
  }

  void _lvTeardown() {
    if (_lvRoot) {
      lv_obj_del(_lvRoot);      // invalidates the covered area → repaint below
      _lvRoot = nullptr;
      _lvList = nullptr;
    }
    _lvPrevSel     = -1;
    _lvNeedRebuild = true;
  }

  void _lvBuild() {
    LvglUi::ensure();
    _lvRoot = LvglUi::pageRoot(title());
    _lvList = LvglUi::makeList(_lvRoot);
    _lvNeedRebuild = true;
  }

  void _lvRender() {
    if (!_lvRoot) _lvBuild();
    if (_lvNeedRebuild) { _lvPopulateRows(); _lvNeedRebuild = false; }
    else _lvRefreshRows();
    _lvApplyFocus();
    lv_timer_handler();
  }

  void _lvPopulateRows() {
    lv_obj_clean(_lvList);
    uint8_t eff = _effectiveCount();
    for (uint8_t i = 0; i < eff; i++) {
      const ListItem& it = _items[i];
      const bool stacked = _stackedSublabels && it.sublabel;
      lv_obj_t* row = lv_btn_create(_lvList);
      lv_obj_set_size(row, lv_pct(100), stacked ? 44 : 34);
      LvglUi::styleRow(row);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t* lbl = lv_label_create(row);
      lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
      lv_obj_set_width(lbl, lv_pct(stacked ? 92 : (it.sublabel ? 58 : 92)));
      lv_obj_set_pos(lbl, 6, stacked ? 2 : 8);
      lv_label_set_text(lbl, it.label ? it.label : "");
      if (it.hasRssi)
        lv_obj_set_style_text_color(lbl, _c565(_rssiColor(it.rssi)), 0);

      if (it.sublabel) {
        lv_obj_t* sub = lv_label_create(row);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x909090), 0);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        if (stacked) {
          lv_obj_set_pos(sub, 6, 25);
          lv_obj_set_width(sub, lv_pct(92));
        } else {
          lv_obj_align(sub, LV_ALIGN_RIGHT_MID, -6, 0);
          lv_obj_set_width(sub, lv_pct(38));
        }
        lv_label_set_text(sub, it.sublabel);
      }
    }
    _lvPrevSel = -1;
    _lvApplyFocus();
  }

  // In-place item edits (sublabels toggled by a screen, item text changes):
  // update existing rows' texts without rebuilding the tree.
  void _lvRefreshRows() {
    if (!_lvList) return;
    uint8_t eff = _effectiveCount();
    for (uint8_t i = 0; i < eff; i++) {
      lv_obj_t* row = lv_obj_get_child(_lvList, i);
      if (!row) break;
      const ListItem& it = _items[i];
      lv_obj_t* lbl = lv_obj_get_child(row, 0);
      if (lbl && it.label) lv_label_set_text(lbl, it.label);
      if (it.sublabel) {
        lv_obj_t* sub = lv_obj_get_child(row, 1);
        if (sub) lv_label_set_text(sub, it.sublabel);
      }
    }
  }

  void _lvApplyFocus() {
    if (!_lvList) return;
    uint8_t eff = _effectiveCount();
    if (eff == 0) return;
    if (_selectedIndex >= eff) _selectedIndex = eff - 1;
    if (_lvPrevSel == (int)_selectedIndex) return;
    if (_lvPrevSel >= 0 && _lvPrevSel < (int)eff && _lvPrevSel != (int)_selectedIndex) {
      lv_obj_t* prev = lv_obj_get_child(_lvList, _lvPrevSel);
      if (prev) lv_obj_clear_state(prev, LV_STATE_FOCUSED);
    }
    lv_obj_t* cur = lv_obj_get_child(_lvList, _selectedIndex);
    if (cur) {
      lv_obj_add_state(cur, LV_STATE_FOCUSED);
      // ANIM_OFF: animated scroll repaints the whole list per frame — at the
      // 4 MHz SPI bus that reads as flicker.
      lv_obj_scroll_to_view(cur, LV_ANIM_OFF);
    }
    _lvPrevSel = _selectedIndex;
  }
#endif // USE_LVGL_SHELL

  void _scrollIfNeeded()
  {
#ifdef USE_LVGL_SHELL
    _lvNeedRebuild = true;
#endif
    uint8_t visible = bodyH() / ITEM_H;
    uint8_t eff     = _effectiveCount();
    if (_selectedIndex < _scrollOffset) {
      _scrollOffset     = _selectedIndex;
      _partialTopActive = false;
    } else if (_selectedIndex >= _scrollOffset + visible) {
      _scrollOffset     = _selectedIndex - visible + 1;
      _partialTopActive = true;
    }
    (void)eff;
  }
};
