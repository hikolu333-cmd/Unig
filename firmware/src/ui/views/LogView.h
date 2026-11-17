#pragma once

#include "core/IDisplay.h"
#include "ui/lvgl/LvglUi.h"   // LvglUi::pump() is a no-op without USE_LVGL_SHELL
#include "ui/components/TagPrompt.h"

// Reusable scrolling log buffer.
// Status bar content is drawn via a callback so each screen can customize it.
// Dual-mode:
//  - legacy: sprite rows pushed to the panel;
//  - USE_LVGL_SHELL: flex-column of coloured labels inside an LVGL
//    container; the status-bar callback draws into a TFT_eSprite that is
//    byte-swapped into an lv_canvas (sprites store little-endian RGB565,
//    lv_color_t storage is big-endian under LV_COLOR_16_SWAP).
class LogView {
public:
  using StatusBarCallback = void (*)(Sprite& sp, int barY, int width, void* userData);

  void clear() { _count = 0; _scrollY = 0; _dirty = true; }
  void resetScroll() { _scrollY = 0; }

  void scroll(int delta) {
    _scrollY += delta;
    if (_scrollY < 0) _scrollY = 0;
    if (_scrollY > _count) _scrollY = _count;
  }

  void addLine(const char* msg, uint16_t color = TFT_WHITE) {
    if (_count < MAX_LINES) {
      strncpy(_lines[_count], msg, LINE_LEN - 1);
      _lines[_count][LINE_LEN - 1] = '\0';
      _colors[_count] = color;
      _count++;
    } else {
      for (int i = 0; i < MAX_LINES - 1; i++) {
        memcpy(_lines[i], _lines[i + 1], LINE_LEN);
        _colors[i] = _colors[i + 1];
      }
      strncpy(_lines[MAX_LINES - 1], msg, LINE_LEN - 1);
      _lines[MAX_LINES - 1][LINE_LEN - 1] = '\0';
      _colors[MAX_LINES - 1] = color;
    }
    _dirty = true;
  }

#ifdef USE_LVGL_SHELL
  static constexpr int LINE_H = 16;   // montserrat_14 row height

  ~LogView() {
    // LVGL widgets are children of the page root and die with it; only the
    // status-bar bridge sprite is owned here.
    if (_statusSpr) { _statusSpr->deleteSprite(); delete _statusSpr; _statusSpr = nullptr; }
  }

  void draw(IDisplay& lcd, int x, int y, int w, int h,
            StatusBarCallback statusCb = nullptr, void* userData = nullptr, int statusLines = 1)
  {
    const int statusH = 11 * (statusLines < 1 ? 1 : statusLines);
    int cbH      = statusCb ? statusH + 1 : 0;
    int logAreaH = h - cbH;
    if (logAreaH < 0) logAreaH = 0;

    if (!_cont) _build();
    // A live TagPrompt layer (waiting for tag/reader) yields the body back
    // the moment real content is drawn.
    TagPrompt::clear();
    // viewParent() sits at the page's body top — map absolute body coords.
    lv_obj_set_pos(_cont, x, y - LvglUi::viewOffsetY());
    lv_obj_set_size(_cont, w, h);
    lv_obj_set_size(_log, w, logAreaH);
    if (_dirty) { _rebuildLines(); _dirty = false; }

    // Follow the latest line unless the user scrolled up (_scrollY lines).
    int maxScroll = _count * LINE_H - logAreaH;
    if (maxScroll < 0) maxScroll = 0;
    int target = maxScroll - _scrollY * LINE_H;
    if (target < 0) target = 0;
    lv_obj_scroll_to_y(_log, target, LV_ANIM_OFF);

    if (statusCb) {
      lv_obj_set_pos(_sep, 0, logAreaH);
      lv_obj_set_size(_sep, w, 1);
      lv_obj_set_pos(_canvas, 0, logAreaH + 1);
      _renderStatus(lcd, statusCb, w, statusH, userData);
    }
  }
#else
  void draw(IDisplay& lcd, int x, int y, int w, int h,
            StatusBarCallback statusCb = nullptr, void* userData = nullptr, int statusLines = 1)
  {
    static constexpr int lineH   = 10;
    static constexpr int statusLineH = 11;
    const int statusH = statusLineH * (statusLines < 1 ? 1 : statusLines);
    _scrollY = 0;
    // +1 for the separator hline that sits between log area and status bar.
    int cbH      = statusCb ? statusH + 1 : 0;
    int logAreaH = h - cbH;
    if (logAreaH < 0) logAreaH = 0;
    int maxVisible = logAreaH / lineH;
    int autoStart  = _count > maxVisible ? _count - maxVisible : 0;
    int startIdx   = autoStart - _scrollY;
    if (startIdx < 0) startIdx = 0;

    // One sprite reused for every row, not one per row. At 240 px wide each is
    // a ~4.8 KB allocation, and creating and freeing a dozen of them on every
    // redraw churns the heap hard enough to matter: on a board without PSRAM
    // the WiFi driver takes its TX buffers from the same heap, and a MITM run
    // serving a portal page was bottoming out at 1 KB free. Same pixels, same
    // flicker-free fill→push per row, one allocation instead of a dozen.
    int rendered = 0;
    if (maxVisible > 0 && startIdx < _count) {
      Sprite sp(&lcd);
      sp.createSprite(w, lineH);
      sp.setTextSize(1);
      sp.setTextDatum(TL_DATUM);
      for (int i = startIdx; i < _count && rendered < maxVisible; i++) {
        int rowY = y + (i - startIdx) * lineH;
        sp.fillSprite(TFT_BLACK);
        sp.setTextColor(_colors[i], TFT_BLACK);
        sp.drawString(_lines[i], 2, 0);
        sp.pushSprite(x, rowY);
        rendered++;
      }
      sp.deleteSprite();
    }

    // Clear any unused rows below the last log line.
    int usedH = rendered * lineH;
    if (usedH < logAreaH)
      lcd.fillRect(x, y + usedH, w, logAreaH - usedH, TFT_BLACK);

    // Status bar — small sprite so the callback interface stays unchanged.
    if (statusCb) {
      int sepY = y + logAreaH;
      lcd.drawFastHLine(x, sepY, w, TFT_DARKGREY);

      Sprite sp(&lcd);
      sp.createSprite(w, statusH);
      sp.fillSprite(TFT_BLACK);
      statusCb(sp, 3, w, userData);
      sp.pushSprite(x, sepY + 1);
      sp.deleteSprite();
    }
  }
#endif

  int count() const { return _count; }

private:
  static constexpr int MAX_LINES = 30;
  static constexpr int LINE_LEN  = 60;
  char _lines[MAX_LINES][LINE_LEN] = {};
  uint16_t _colors[MAX_LINES] = {};
  int _count   = 0;
  int _scrollY = 0;  // lines scrolled up from auto-scroll position; 0 = follow latest
  bool _dirty  = false;   // shared: LVGL rebuild flag / legacy redraw hint
#ifdef USE_LVGL_SHELL

  lv_obj_t* _cont    = nullptr;   // outer container at (x,y,w,h)
  lv_obj_t* _log     = nullptr;   // scrolling flex-column of line labels
  lv_obj_t* _sep     = nullptr;   // separator hline above the status bar
  lv_obj_t* _canvas  = nullptr;   // status bar (sprite → canvas bridge)
  Sprite*   _statusSpr = nullptr; // backing sprite the callback draws into
  uint16_t* _statusBuf = nullptr; // byte-swapped copy for LVGL

  void _build() {
    LvglUi::ensure();
    _cont = lv_obj_create(LvglUi::viewParent());
    lv_obj_set_style_bg_opa(_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_cont, 0, 0);
    lv_obj_set_style_pad_all(_cont, 0, 0);
    lv_obj_clear_flag(_cont, LV_OBJ_FLAG_SCROLLABLE);

    _log = lv_obj_create(_cont);
    lv_obj_set_pos(_log, 0, 0);
    lv_obj_set_style_bg_opa(_log, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_log, 0, 0);
    lv_obj_set_style_pad_all(_log, 0, 0);
    lv_obj_set_style_pad_row(_log, 0, 0);
    lv_obj_set_flex_flow(_log, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(_log, LV_SCROLLBAR_MODE_OFF);

    _sep = lv_obj_create(_cont);
    lv_obj_set_style_bg_opa(_sep, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_sep, LvglUi::c565(TFT_DARKGREY), 0);
    lv_obj_set_style_border_width(_sep, 0, 0);
    lv_obj_set_style_radius(_sep, 0, 0);
    lv_obj_clear_flag(_sep, LV_OBJ_FLAG_SCROLLABLE);

    _canvas = lv_canvas_create(_cont);
  }

  void _rebuildLines() {
    lv_obj_clean(_log);
    for (int i = 0; i < _count; i++) {
      lv_obj_t* l = lv_label_create(_log);
      lv_obj_set_width(l, lv_pct(100));
      lv_obj_set_height(l, LINE_H);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, LvglUi::c565(_colors[i]), 0);
      lv_label_set_text(l, _lines[i]);
    }
  }

  // The legacy status-bar callback draws into a Sprite — replicate that into
  // a TFT_eSprite, then byte-swap the pixels into the canvas buffer.
  void _renderStatus(IDisplay& lcd, StatusBarCallback statusCb, int w, int statusH, void* userData) {
    if (!_statusSpr) _statusSpr = new Sprite(&lcd);
    if (!_statusSpr->created() || _statusSpr->width() != w || _statusSpr->height() != statusH) {
      _statusSpr->deleteSprite();
      if (!_statusSpr->createSprite(w, statusH)) return;
      _statusBuf = (uint16_t*)realloc(_statusBuf, w * statusH * sizeof(uint16_t));
      lv_canvas_set_buffer(_canvas, _statusBuf, w, statusH, LV_IMG_CF_TRUE_COLOR);
    }
    _statusSpr->fillSprite(TFT_BLACK);
    statusCb(*_statusSpr, 3, w, userData);

    const uint16_t* src = _statusSpr->pixels();
    const int n = w * statusH;
    for (int i = 0; i < n; i++) {
      uint16_t v = src[i];
      _statusBuf[i] = (uint16_t)((v >> 8) | (v << 8));
    }
    lv_obj_invalidate(_canvas);
  }
#endif
};
