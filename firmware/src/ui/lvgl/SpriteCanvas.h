#pragma once
#ifdef USE_LVGL_SHELL

#include "ui/lvgl/LvglUi.h"
#include "core/IDisplay.h"

// Full-region sprite→canvas bridge for pixel-art screens (mascots, games).
// The scene is drawn into a Sprite (the project's dual-backend alias:
// CaptureSprite under TFT_eSPI, LGFX_Sprite under M5GFX), then the pixels
// are byte-swapped into an lv_canvas buffer (sprites store little-endian
// RGB565; LVGL canvas storage is big-endian under LV_COLOR_16_SWAP) and the
// canvas is invalidated. One sprite + one buffer allocation per size,
// reused every frame — same bridge LogView uses for its status bar.
class SpriteCanvas {
public:
  ~SpriteCanvas() { destroy(); }

  // (Re)create at w×h if the size changed. Returns false on allocation
  // failure (caller should skip the frame rather than draw).
  bool ensure(IDisplay& lcd, int w, int h) {
    return ensure(lcd, LvglUi::viewParent(), w, h);
  }

  // Same, with an explicit parent (e.g. the shell's home page) — the default
  // overload parents to LvglUi::viewParent(), which legacy screens own.
  bool ensure(IDisplay& lcd, lv_obj_t* parent, int w, int h) {
    if (_spr && _spr->created() && _spr->width() == w && _spr->height() == h
        && _canvas && lv_obj_get_parent(_canvas) == parent)
      return true;
    destroy();

    _spr = new Sprite(&lcd);
    _spr->createSprite(w, h);
    if (!_spr->created()) { destroy(); return false; }

    _buf = malloc((size_t)w * h * sizeof(uint16_t));
    if (!_buf) { destroy(); return false; }

    _canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(_canvas, _buf, w, h, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, 0, 0);
    lv_obj_clear_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);
    return true;
  }

  void setPos(int x, int y) { if (_canvas) lv_obj_set_pos(_canvas, x, y); }

  // The sprite to draw this frame into.
  Sprite& sprite() { return *_spr; }

  // Byte-swap the drawn pixels into the canvas and invalidate it.
  void flush() {
    if (!_canvas || !_spr) return;
    const int n = _spr->width() * _spr->height();
    const uint16_t* src = _spr->pixels();
    uint16_t* dst = (uint16_t*)_buf;
    for (int i = 0; i < n; i++) {
      uint16_t v = src[i];
      dst[i] = (uint16_t)((v >> 8) | (v << 8));
    }
    lv_obj_invalidate(_canvas);
  }

  lv_obj_t* obj() { return _canvas; }

  void destroy() {
    if (_spr) { if (_spr->created()) _spr->deleteSprite(); delete _spr; _spr = nullptr; }
    if (_canvas) { lv_obj_del(_canvas); _canvas = nullptr; }
    if (_buf) { free(_buf); _buf = nullptr; }
  }

private:
  Sprite*    _spr    = nullptr;
  lv_obj_t*  _canvas = nullptr;
  void*      _buf    = nullptr;
};

#endif  // USE_LVGL_SHELL
