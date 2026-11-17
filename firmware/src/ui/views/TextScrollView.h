#pragma once

#include "core/Device.h"
#include "core/INavigation.h"
#include "core/ConfigManager.h"
#include "ui/lvgl/LvglUi.h"   // LvglUi::pump() is a no-op without USE_LVGL_SHELL

// Word-wrapped, scrollable text region. Pure view — owner screen calls
// setContent()/updateContent(), then render(x,y,w,h) on draw and onNav(dir)
// on input. Up/Down scroll one line, Left/Right page.
// Dual-mode:
//  - legacy: manual wrap into sprite rows;
//  - USE_LVGL_SHELL: a single LVGL label (LVGL wraps the text itself) in a
//    scrolling container on the current page root.
class TextScrollView
{
public:
  enum WrapMode : uint8_t {
    WRAP_WORD,
    WRAP_CHARACTER,
  };

#ifdef USE_LVGL_SHELL
  static constexpr int LINE_H = 16;   // montserrat_14 row height

  // WRAP_CHARACTER (terminal transcripts) keeps every column position: LVGL
  // word-wraps, breaking overlong words at any character — good enough for
  // both modes, so the mode is only honored by the legacy renderer.

  void setWrapMode(WrapMode mode) { (void)mode; }

  void setContent(const String& content)
  {
    _content = content;
    _pendingBottom = false;
    _dirty = true;
    if (_cont) lv_obj_scroll_to_y(_cont, 0, LV_ANIM_OFF);   // fresh content starts at the top
  }

  // Replace content without necessarily resetting the user's scroll position.
  // followBottom=true is intended for live logs/terminals that should keep
  // following new content while the user is already at the bottom.
  void updateContent(const String& content, bool followBottom = false)
  {
    if (content == _content) {
      if (followBottom) _pendingBottom = true;
      return;
    }

    bool atBottom = isAtBottom();
    _content = content;
    _dirty = true;
    _pendingBottom = followBottom && atBottom;
  }

  void resetScroll()
  {
    if (_cont) lv_obj_scroll_to_y(_cont, 0, LV_ANIM_OFF);
    _pendingBottom = false;
  }

  void scrollToBottom()
  {
    if (!_cont) { _pendingBottom = true; return; }
    lv_obj_scroll_to_y(_cont, LV_COORD_MAX, LV_ANIM_OFF);
    _pendingBottom = false;
  }

  bool isAtBottom() const
  {
    if (!_cont) return true;
    // Scroll position is within one line of the end → "at bottom".
    lv_coord_t y = lv_obj_get_scroll_y(_cont);
    return y >= _maxScroll() - LINE_H;
  }

  void render(int x, int y, int w, int h)
  {
    if (!_cont) _build();
    // viewParent() sits at the page's body top — map absolute body coords.
    lv_obj_set_pos(_cont, x, y - LvglUi::viewOffsetY());
    lv_obj_set_size(_cont, w, h);
    if (_dirty) {
      lv_label_set_text(_label, _content.c_str());
      _dirty = false;
    }
    if (_pendingBottom) scrollToBottom();
  }

  bool onNav(INavigation::Direction dir)
  {
    if (!_cont) return false;
    int visible = _visibleLines();
    int maxOffset = _maxScroll();

    if (dir == INavigation::DIR_UP && lv_obj_get_scroll_y(_cont) > 0) {
      lv_obj_scroll_by(_cont, 0, LINE_H, LV_ANIM_OFF);
      return true;
    }
    if (dir == INavigation::DIR_DOWN && lv_obj_get_scroll_y(_cont) < maxOffset) {
      lv_obj_scroll_by(_cont, 0, -LINE_H, LV_ANIM_OFF);
      return true;
    }
    if (dir == INavigation::DIR_LEFT && lv_obj_get_scroll_y(_cont) > 0) {
      lv_obj_scroll_by(_cont, 0, visible * LINE_H, LV_ANIM_OFF);
      return true;
    }
    if (dir == INavigation::DIR_RIGHT && lv_obj_get_scroll_y(_cont) < maxOffset) {
      lv_obj_scroll_by(_cont, 0, -visible * LINE_H, LV_ANIM_OFF);
      return true;
    }
    return false;
  }

private:
  String _content;
  bool   _dirty = true;
  bool   _pendingBottom = false;
  lv_obj_t* _cont  = nullptr;
  lv_obj_t* _label = nullptr;

  int _visibleLines() const
  {
    return _cont ? max(1, (int)lv_obj_get_height(_cont) / LINE_H) : 1;
  }

  // Greatest scroll offset: content height (label) minus the viewport.
  int _maxScroll() const
  {
    if (!_cont) return 0;
    int contentH = _label ? lv_obj_get_height(_label) : 0;
    int maxOff = contentH - (int)lv_obj_get_height(_cont);
    return maxOff > 0 ? maxOff : 0;
  }

  void _build() {
    LvglUi::ensure();
    _cont = lv_obj_create(LvglUi::viewParent());
    lv_obj_set_style_bg_opa(_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_cont, 0, 0);
    lv_obj_set_style_pad_all(_cont, 2, 0);
    lv_obj_set_scrollbar_mode(_cont, LV_SCROLLBAR_MODE_OFF);

    _label = lv_label_create(_cont);
    lv_obj_set_width(_label, lv_pct(100));
    lv_label_set_long_mode(_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_label, lv_color_white(), 0);
    lv_label_set_text(_label, "");
  }

#else // ── legacy TFT mode ───────────────────────────────────────────────────

  void setWrapMode(WrapMode mode)
  {
    if (_wrapMode == mode) return;
    _wrapMode = mode;
    _wrapped = false;
  }

  void setContent(const String& content)
  {
    _content = content;
    _scrollOffset = 0;
    _scrollToBottomPending = false;
    _discardedRows = 0;
    _wrapped = false;  // re-wrap on next render — body width may have changed
  }

  // Replace content without necessarily resetting the user's scroll position.
  // followBottom=true is intended for live logs/terminals that should keep
  // following new content while the user is already at the bottom.
  void updateContent(const String& content, bool followBottom = false)
  {
    if (content == _content) {
      if (followBottom) _scrollToBottomPending = true;
      return;
    }

    _content = content;
    _wrapped = false;
    _scrollToBottomPending = followBottom;
  }

  void resetScroll()
  {
    _scrollOffset = 0;
    _scrollToBottomPending = false;
  }

  void scrollToBottom()
  {
    if (!_wrapped) {
      _scrollToBottomPending = true;
      return;
    }

    _scrollOffset = _maxOffset();
    _scrollToBottomPending = false;
    if (_w > 0 && _h > 0) _draw();
  }

  bool isAtBottom() const
  {
    return _scrollOffset >= _maxOffset();
  }

  void render(int x, int y, int w, int h)
  {
    _x = x; _y = y; _w = w; _h = h;
    if (!_wrapped) _wrapText();
    _draw();
  }

  bool onNav(INavigation::Direction dir)
  {
    int visible = _visibleLines();
    int maxOffset = _maxOffset();

    if (dir == INavigation::DIR_UP && _scrollOffset > 0) {
      _scrollOffset--;
      _draw();
      return true;
    }
    if (dir == INavigation::DIR_DOWN && _scrollOffset < maxOffset) {
      _scrollOffset++;
      _draw();
      return true;
    }
    if (dir == INavigation::DIR_LEFT && _scrollOffset > 0) {
      _scrollOffset = (_scrollOffset > visible) ? _scrollOffset - visible : 0;
      _draw();
      return true;
    }
    if (dir == INavigation::DIR_RIGHT && _scrollOffset < maxOffset) {
      _scrollOffset += visible;
      if (_scrollOffset > maxOffset) _scrollOffset = maxOffset;
      _draw();
      return true;
    }
    return false;
  }

private:
  static constexpr int kLineH    = 11;
  static constexpr int kScrollW  = 3;
  static constexpr int kMaxLines = 160;

  String _content;
  String _lines[kMaxLines];
  int    _lineCount    = 0;
  int    _scrollOffset = 0;
  int    _x = 0, _y = 0, _w = 0, _h = 0;
  bool   _wrapped      = false;
  WrapMode _wrapMode    = WRAP_WORD;
  bool   _scrollToBottomPending = false;
  int    _discardedRows = 0;
  int    _wrapDiscarded = 0;

  int _visibleLines() const
  {
    int visible = (_h > 0) ? _h / kLineH : 1;
    return max(1, visible);
  }

  int _maxOffset() const
  {
    int visible = _visibleLines();
    return (_lineCount > visible) ? _lineCount - visible : 0;
  }

  void _pushWrappedLine(const String& line)
  {
    if (_lineCount < kMaxLines) {
      _lines[_lineCount++] = line;
      return;
    }

    // Keep the newest rows. _wrapText() later compensates the scroll offset
    // by the number of newly discarded rows so browsing stays stable.
    for (int i = 1; i < kMaxLines; i++) _lines[i - 1] = _lines[i];
    _lines[kMaxLines - 1] = line;
    _wrapDiscarded++;
  }

  void _wrapText()
  {
    int oldDiscarded = _discardedRows;
    _wrapDiscarded = 0;
    _lineCount = 0;
    int maxChars = (_w - kScrollW - 8) / 6;
    if (maxChars < 8) maxChars = 8;

    int pos = 0;
    int len = (int)_content.length();
    while (pos <= len) {
      int nl = _content.indexOf('\n', pos);
      int segEnd = (nl < 0) ? len : nl;

      if (pos == segEnd) {
        _pushWrappedLine("");
      } else {
        while (pos < segEnd) {
          int remain = segEnd - pos;
          if (remain <= maxChars) {
            _pushWrappedLine(_content.substring(pos, segEnd));
            pos = segEnd;
            break;
          }

          int wrapAt = pos + maxChars;

          // Terminal/transcript mode must preserve every character position:
          // wrapping at whitespace would consume the separator and distort
          // column-oriented output such as `ls`. Other users retain the
          // existing word-wrap behavior by default.
          if (_wrapMode == WRAP_CHARACTER) {
            _pushWrappedLine(_content.substring(pos, wrapAt));
            pos = wrapAt;
            continue;
          }

          int lastSpace = -1;
          for (int i = wrapAt; i > pos; i--) {
            if (_content[i] == ' ') { lastSpace = i; break; }
          }

          if (lastSpace > pos) {
            _pushWrappedLine(_content.substring(pos, lastSpace));
            pos = lastSpace + 1;
          } else {
            _pushWrappedLine(_content.substring(pos, wrapAt));
            pos = wrapAt;
          }
        }
      }

      if (nl < 0) break;
      pos = nl + 1;
    }

    _wrapped = true;

    int maxOffset = _maxOffset();
    if (_scrollToBottomPending) {
      _scrollOffset = maxOffset;
      _scrollToBottomPending = false;
    } else {
      int newlyDiscarded = _wrapDiscarded - oldDiscarded;
      if (newlyDiscarded > 0)
        _scrollOffset = max(0, _scrollOffset - newlyDiscarded);
      if (_scrollOffset > maxOffset) _scrollOffset = maxOffset;
    }
    _discardedRows = _wrapDiscarded;
  }

  void _draw()
  {
    auto& lcd = Uni.Lcd;
    int visible = _visibleLines();
    int textColW = _w - kScrollW;

    int rendered = 0;
    for (int i = 0; i < visible; i++) {
      int idx = i + _scrollOffset;
      if (idx >= _lineCount) break;

      Sprite sp(&lcd);
      sp.createSprite(textColW, kLineH);
      sp.fillSprite(TFT_BLACK);
      sp.setTextSize(1);
      sp.setTextDatum(TL_DATUM);
      sp.setTextColor(TFT_WHITE, TFT_BLACK);
      sp.drawString(_lines[idx].c_str(), 4, 1);
      sp.pushSprite(_x, _y + i * kLineH);
      sp.deleteSprite();
      rendered++;
    }

    int usedH = rendered * kLineH;
    if (usedH < _h) {
      lcd.fillRect(_x, _y + usedH, textColW, _h - usedH, TFT_BLACK);
    }

    int sbX = _x + _w - kScrollW;
    lcd.fillRect(sbX, _y, kScrollW, _h, 0x2104);
    if (_lineCount > visible) {
      int sbH = max(4, _h * visible / _lineCount);
      if (sbH > _h) sbH = _h;
      int maxOffset = _lineCount - visible;
      int sbY = (_h - sbH) * _scrollOffset / max(1, maxOffset);
      if (sbY < 0)              sbY = 0;
      if (sbY > _h - sbH)       sbY = _h - sbH;
      lcd.fillRect(sbX, _y + sbY, kScrollW, sbH, Config.getThemeColor());
    }
  }
#endif
};
