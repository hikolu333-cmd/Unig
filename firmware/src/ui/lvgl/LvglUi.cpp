//
// LvglUi — see header.
//

#include "ui/lvgl/LvglUi.h"

#ifdef USE_LVGL_SHELL

#include <Arduino.h>
#include "core/ConfigManager.h"
#include "core/Device.h"

namespace {

bool        s_init = false;
lv_style_t s_styleRow;
lv_style_t s_styleRowFocus;

lv_color_t _accent() {
  uint16_t c = Config.getThemeColor();
  return lv_color_make((uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
                       (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
                       (uint8_t)((c & 0x1F) * 255 / 31));
}

// LVGL draw buffer -> panel. lv_color_t is big-endian in memory
// (LV_COLOR_16_SWAP 1), which is exactly the byte order TFT_eSPI's
// pushImage() puts on the SPI wire — same convention as TFT_eSprite.
void _flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  Uni.Lcd.pushImage(area->x1, area->y1, w, h, (const uint16_t*)color_p);
  lv_disp_flush_ready(drv);
}

} // namespace

namespace LvglUi
{

void ensure() {
  if (s_init) return;

  lv_init();

  const int W = Uni.Lcd.width();
  const int H = Uni.Lcd.height();
  const int bufH = 40;
  static lv_disp_draw_buf_t drawBuf;
  static lv_color_t* bufPx = (lv_color_t*)malloc(W * bufH * sizeof(lv_color_t));
  lv_disp_draw_buf_init(&drawBuf, bufPx, nullptr, W * bufH);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res  = W;
  dispDrv.ver_res = H;
  dispDrv.flush_cb = _flush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_t* disp = lv_disp_drv_register(&dispDrv);

  lv_theme_t* th = lv_theme_default_init(disp, _accent(),
                                         lv_palette_darken(LV_PALETTE_GREY, 4),
                                         true, &lv_font_montserrat_16);
  lv_disp_set_theme(disp, th);

  // Shared row / button styles (also used by the shell main menu).
  lv_style_init(&s_styleRow);
  lv_style_set_bg_color(&s_styleRow, lv_color_hex(0x141414));
  lv_style_set_bg_opa(&s_styleRow, LV_OPA_COVER);
  lv_style_set_radius(&s_styleRow, 6);
  lv_style_set_border_color(&s_styleRow, lv_color_hex(0x3A3A3A));
  lv_style_set_border_width(&s_styleRow, 1);
  lv_style_set_text_color(&s_styleRow, lv_color_hex(0xC8C8C8));
  lv_style_set_pad_all(&s_styleRow, 0);

  lv_style_init(&s_styleRowFocus);
  lv_style_set_bg_color(&s_styleRowFocus, _accent());
  lv_style_set_bg_opa(&s_styleRowFocus, LV_OPA_COVER);
  lv_style_set_border_color(&s_styleRowFocus, lv_color_white());
  lv_style_set_border_width(&s_styleRowFocus, 2);
  lv_style_set_text_color(&s_styleRowFocus, lv_color_black());

  s_init = true;
}

lv_obj_t* pageRoot(const char* title) {
  ensure();
  lv_obj_t* root = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_size(root, Uni.Lcd.width(), Uni.Lcd.height());
  lv_obj_set_style_bg_color(root, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(root, 0, 0);
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);

  if (title) {
    lv_obj_t* t = lv_label_create(root);
    lv_obj_set_pos(t, 8, 5);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, _accent(), 0);
    lv_label_set_text(t, title);
  }
  return root;
}

lv_obj_t* makeList(lv_obj_t* root) {
  lv_obj_t* list = lv_obj_create(root);
  lv_obj_set_pos(list, 4, 34);
  lv_obj_set_size(list, Uni.Lcd.width() - 8, Uni.Lcd.height() - 38);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 4, 0);
  // Scrollable for long lists, but the scrollbar itself is hidden: a 3px
  // accent-coloured bar reads as a stray stripe on the panel.
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  return list;
}

void styleRow(lv_obj_t* row) {
  lv_obj_add_style(row, &s_styleRow, 0);
  lv_obj_add_style(row, &s_styleRowFocus, LV_STATE_FOCUSED);
}

} // namespace LvglUi

#endif // USE_LVGL_SHELL
