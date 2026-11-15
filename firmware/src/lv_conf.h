//
// lv_conf.h — LVGL configuration for the USE_LVGL_SHELL boards (diy_joystick).
// Everything not set here falls back to LVGL 8.3 internal defaults
// (see lvgl/src/lv_conf_internal.h).
//
#ifndef LV_CONF_H
#define LV_CONF_H

/* Color depth: match the panel (ILI9341-family RGB565).
   LV_COLOR_16_SWAP 1: lv_color_t is stored big-endian ([hi, lo] per pixel) —
   exactly the byte order TFT_eSPI::pushImage() expects on the SPI wire
   (same convention TFT_eSprite uses internally, see Sprite.cpp). With 0 the
   flush sends little-endian pixels and every colour's channels come out
   scrambled (illegible text, mushy images). */
#define LV_COLOR_DEPTH       16
#define LV_COLOR_16_SWAP     1

/* Memory: LVGL's own TLSF pool in .bss (board has no PSRAM; 48K is plenty for
   the shell UI). The draw buffer is allocated separately (see LvglShell.cpp). */
#define LV_MEM_CUSTOM        0
#define LV_MEM_SIZE          (48U * 1024U)

/* Tick: drive LVGL time straight from Arduino millis() — no lv_tick_inc()
   needed in the loop. */
#define LV_TICK_CUSTOM       1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Fonts: default UI font + a bigger one for headings/numbers. 16 px default
   reads much better on the 2.8" 320x240 panel than 14 px. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT      &lv_font_montserrat_16

/* Layout: flex powers the menu grid. */
#define LV_USE_FLEX          1

/* Keep the runtime monitors off (flash + cycles). */
#define LV_USE_PERF_MONITOR  0
#define LV_USE_MEM_MONITOR   0

/* Extras we don't use — off to save flash. */
#define LV_USE_PNG           0
#define LV_USE_BMP           0
#define LV_USE_FREETYPE      0
#define LV_USE_FS_STDIO      0
#define LV_USE_FS_POSIX      0
#define LV_USE_FS_WIN32      0
#define LV_USE_FS_FATFS      0
#define LV_USE_SJPG          0
#define LV_USE_GIF           0
#define LV_USE_QRCODE        0
#define LV_USE_FRAGMENT      0
#define LV_USE_IMGFONT       0
#define LV_USE_SNAPSHOT      0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_VECTOR_GRAPHIC 0

#endif /* LV_CONF_H */
