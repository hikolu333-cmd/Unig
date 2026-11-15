//
// LvglShell — see header. LVGL 8.3 + TFT_eSPI (via Uni.Lcd.pushImage, which
// also feeds the web ScreenMirror), no touch: navigation comes from
// INavigation (stick / PC remote).
//

#include "ui/lvgl/LvglShell.h"

#ifdef USE_LVGL_SHELL

#include <lvgl.h>

#include "core/AchievementManager.h"
#include "core/ConfigManager.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "utils/FirmwareInfo.h"
#include "utils/Mascot.h"           // Mascot registry + hackerGetRank
#include "ui/lvgl/BoykisserImg.h"

// Legacy screens launched from the LVGL menu — same set as MainMenuScreen.
#include "screens/wifi/WifiMenuScreen.h"
#include "screens/ble/BLEMenuScreen.h"
#include "screens/hid/KeyboardMenuScreen.h"
#include "screens/module/ModuleMenuScreen.h"
#include "screens/utility/UtilityMenuScreen.h"
#include "screens/setting/SettingScreen.h"
#ifdef MINI_BUILD
  #define SHELL_MENU_GAMES 0
  #define SHELL_MENU_LUA   0
#else
  #define SHELL_MENU_GAMES 1
  #define SHELL_MENU_LUA   1
#endif
#if SHELL_MENU_LUA
  #include "screens/LuaScreen.h"
#endif
#if SHELL_MENU_GAMES
  #include "screens/game/GameMenuScreen.h"
#endif

// ── shell state (file-scope; one shell instance in practice) ─────────────────
namespace {

bool     s_lvInit      = false;   // lv_init + display driver done
lv_obj_t* s_home       = nullptr;
lv_obj_t* s_menu       = nullptr;

// home widgets
lv_obj_t* s_agentLbl    = nullptr;
lv_obj_t* s_rankLbl     = nullptr;
lv_obj_t* s_expLbl      = nullptr;
lv_obj_t* s_expBar      = nullptr;
lv_obj_t* s_mascot      = nullptr;   // lv_img (boykisser) or fallback badge
lv_obj_t* s_mascotAlt   = nullptr;   // fallback badge for non-photo mascots
lv_obj_t* s_bubblePanel = nullptr;
lv_obj_t* s_bubbleLbl   = nullptr;
lv_obj_t* s_hpLbl       = nullptr;
lv_obj_t* s_hpBar       = nullptr;
lv_obj_t* s_brLbl       = nullptr;
lv_obj_t* s_brBar       = nullptr;
lv_obj_t* s_hintLbl     = nullptr;

lv_img_dsc_t s_imgOpen, s_imgClosed;
int      s_mascotX      = 0;

// menu widgets
lv_obj_t* s_menuTitle   = nullptr;
lv_obj_t* s_menuList    = nullptr;
lv_obj_t* s_menuBtns[8] = {};
int      s_menuSel      = 0;
int      s_menuCount    = 0;

lv_style_t s_styleBtn;
lv_style_t s_styleBtnFocus;
lv_style_t s_styleBubble;

// bubble state machine (typewriter, like the old CharacterScreen one)
const char* const kBubbleWords[] = {
  "DEAUTH SENT", "EVIL TWIN ON", "BEACON SPAM", "EAPOL GRAB!",
  "KARMA ACTIVE", "SSID CLONED", "BLE SPAM ON", "AIRTAG NEAR!",
  "DUCKY RUN!", "HID INJECT", "MIFARE READ", "NFC DUMP OK",
  "TV-B-GONE", "IR CAPTURE", "GPS LOCK!", ">_HACKING",
  "I LIKE BOYS", "KISS ME :3", "NYA~", "BOYKISSER!",
  "SILLY CAT :3",
};
constexpr int kBubbleN = (int)(sizeof(kBubbleWords) / sizeof(kBubbleWords[0]));
uint8_t  s_bubIdx   = 0;
uint8_t  s_bubPos   = 0;
bool     s_bubHold  = false;
uint32_t s_bubT0    = 0;

// head sway (same 12-step loop as the CharacterScreen)
const int8_t kSway[12] = {0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1};

// ── helpers ──────────────────────────────────────────────────────────────────
int _clampPct(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

lv_color_t _c565(uint16_t c) {
  return lv_color_make((uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
                       (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
                       (uint8_t)((c & 0x1F) * 255 / 31));
}
lv_color_t _accent() { return _c565(Config.getThemeColor()); }

// ── LVGL display driver (flush into TFT_eSPI; also feeds the web mirror) ─────
void _flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  Uni.Lcd.pushImage(area->x1, area->y1, w, h, (const uint16_t*)color_p);
  lv_disp_flush_ready(drv);
}

// ── periodic UI work ──────────────────────────────────────────────────────────
void _refreshInfo() {
  int      exp = Achievement.getExp();
  RankInfo ri = hackerGetRank(exp);
  int      hp = _clampPct(Uni.Power.getBatteryPercentage());
  bool     chg = Uni.Power.isCharging();
  if (hp == 0 && !chg) hp = 100;
  uint32_t totalMem = ESP.getHeapSize() + ESP.getPsramSize();
  uint32_t freeMem  = ESP.getFreeHeap() + ESP.getFreePsram();
  int      brain    = totalMem > 0 ? _clampPct((int)(((totalMem - freeMem) * 100) / totalMem)) : 0;
  int      unlocked = Achievement.getTotalUnlocked();

  String agent = Config.get(APP_CONFIG_DEVICE_NAME, APP_CONFIG_DEVICE_NAME_DEFAULT);
  String title = Config.get(APP_CONFIG_AGENT_TITLE, APP_CONFIG_AGENT_TITLE_DEFAULT);

  lv_label_set_text(s_agentLbl, agent.substring(0, 15).c_str());
  char buf[64];
  snprintf(buf, sizeof(buf), "%s  " LV_SYMBOL_OK " %d  Achv %d/%d",
           title.length() > 0 ? title.c_str() : "No Title",
           exp, unlocked, (int)Achievement.catalog().count);
  lv_label_set_text(s_rankLbl, buf);
  lv_obj_set_style_text_color(s_rankLbl, _c565(ri.color), 0);

  snprintf(buf, sizeof(buf), "EXP %d", exp);
  lv_label_set_text(s_expLbl, buf);
  lv_bar_set_value(s_expBar, _clampPct(exp * 100 / 4500), LV_ANIM_ON);

  lv_bar_set_value(s_hpBar, hp, LV_ANIM_ON);
  snprintf(buf, sizeof(buf), "HP %d%%%s", hp, chg ? "+" : "");
  lv_label_set_text(s_hpLbl, buf);

  lv_bar_set_value(s_brBar, brain, LV_ANIM_ON);
  snprintf(buf, sizeof(buf), "BR %d%%", brain);
  lv_label_set_text(s_brLbl, buf);
}

void _infoTimer(lv_timer_t*)  { _refreshInfo(); }

void _blinkTimer(lv_timer_t*) {
  static uint16_t tick = 0;
  if (tick == 24)      lv_img_set_src(s_mascot, &s_imgClosed);
  else if (tick >= 25) { lv_img_set_src(s_mascot, &s_imgOpen); tick = 0; return; }
  tick++;
}

void _swayExec(void* var, int32_t v) {
  (void)var;
  lv_obj_set_x(s_mascot, s_mascotX + kSway[v] * 2);
}

void _bubbleTimer(lv_timer_t*) {
  const char* w = kBubbleWords[s_bubIdx % kBubbleN];
  if (!s_bubHold) {
    if (s_bubPos < (uint8_t)strlen(w)) {
      s_bubPos++;
      char buf[24];
      memcpy(buf, w, s_bubPos); buf[s_bubPos] = '\0';
      lv_label_set_text(s_bubbleLbl, buf);
    } else {
      s_bubHold = true; s_bubT0 = millis();
    }
  } else if (millis() - s_bubT0 > 2500) {
    uint8_t next;
    do { next = (uint8_t)random(kBubbleN); } while (next == s_bubIdx && kBubbleN > 1);
    s_bubIdx = next; s_bubPos = 0; s_bubHold = false;
  }
}

// ── menu ─────────────────────────────────────────────────────────────────────
void _styleBtns(lv_obj_t* b) {
  lv_obj_add_style(b, &s_styleBtn, 0);
  lv_obj_add_style(b, &s_styleBtnFocus, LV_STATE_FOCUSED);
}

void _menuFocus(int idx) {
  lv_obj_clear_state(s_menuBtns[s_menuSel], LV_STATE_FOCUSED);
  s_menuSel = idx;
  lv_obj_add_state(s_menuBtns[s_menuSel], LV_STATE_FOCUSED);
  // ANIM_OFF: an animated scroll here repaints the whole list each frame —
  // at the 4 MHz SPI bus that reads as flicker.
  lv_obj_scroll_to_view(s_menuBtns[s_menuSel], LV_ANIM_OFF);
}

} // namespace

// ── LvglShellScreen ──────────────────────────────────────────────────────────

void LvglShellScreen::_ensureUi() {
  if (s_lvInit) return;

  lv_init();

  static lv_disp_draw_buf_t drawBuf;
  const int W = Uni.Lcd.width();
  const int H = Uni.Lcd.height();
  const int bufH = 40;
  static lv_color_t* bufPx = (lv_color_t*)malloc(W * bufH * sizeof(lv_color_t));
  Serial.printf("[LVGL] color=%d bytes, swap=%d, buf=%d px (%u B), heap=%u\n",
                (int)sizeof(lv_color_t), (int)LV_COLOR_16_SWAP, W * bufH,
                (unsigned)(W * bufH * sizeof(lv_color_t)),
                (unsigned)ESP.getFreeHeap());
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
                                         true, LV_FONT_DEFAULT);
  lv_disp_set_theme(disp, th);

  // mascot image descriptors (runtime fill: bitfield structs)
  s_imgOpen.header.always_zero = 0;
  s_imgOpen.header.cf  = LV_IMG_CF_TRUE_COLOR_ALPHA;
  s_imgOpen.header.w    = BK_LV_W;
  s_imgOpen.header.h    = BK_LV_H;
  s_imgOpen.data_size   = BK_LV_W * BK_LV_H * 3;  // TRUE_COLOR_ALPHA @ depth 16: 3 B/px
  s_imgOpen.data        = (const uint8_t*)BK_LV_OPEN;
  s_imgClosed = s_imgOpen;
  s_imgClosed.data     = (const uint8_t*)BK_LV_CLOSED;

  // ── styles ──────────────────────────────────────────────────────────────
  lv_style_init(&s_styleBtn);
  lv_style_set_bg_color(&s_styleBtn, lv_color_hex(0x101010));
  lv_style_set_bg_opa(&s_styleBtn, LV_OPA_COVER);
  lv_style_set_radius(&s_styleBtn, 8);
  lv_style_set_border_color(&s_styleBtn, lv_color_hex(0x3A3A3A));
  lv_style_set_border_width(&s_styleBtn, 1);
  lv_style_set_text_color(&s_styleBtn, lv_color_hex(0xC8C8C8));

  lv_style_init(&s_styleBtnFocus);
  lv_style_set_bg_color(&s_styleBtnFocus, _accent());
  lv_style_set_bg_opa(&s_styleBtnFocus, LV_OPA_COVER);
  lv_style_set_border_color(&s_styleBtnFocus, lv_color_white());
  lv_style_set_border_width(&s_styleBtnFocus, 2);
  lv_style_set_text_color(&s_styleBtnFocus, lv_color_black());

  lv_style_init(&s_styleBubble);
  lv_style_set_bg_color(&s_styleBubble, lv_color_hex(0x18181A));
  lv_style_set_bg_opa(&s_styleBubble, LV_OPA_COVER);
  lv_style_set_radius(&s_styleBubble, 10);
  lv_style_set_border_color(&s_styleBubble, _accent());
  lv_style_set_border_width(&s_styleBubble, 1);
  lv_style_set_pad_all(&s_styleBubble, 6);

  // ── home page ───────────────────────────────────────────────────────────
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  s_home = lv_obj_create(scr);
  lv_obj_set_size(s_home, W, H);
  lv_obj_set_pos(s_home, 0, 0);
  lv_obj_set_style_bg_color(s_home, lv_color_black(), 0);
  lv_obj_set_style_border_width(s_home, 0, 0);
  // The default theme gives lv_obj containers ~10px inner padding, which
  // shifts absolutely-placed children and pushes bottom-aligned widgets
  // (hint) onto the HP/BR bars. Our layout is hand-positioned: kill it.
  lv_obj_set_style_pad_all(s_home, 0, 0);
  lv_obj_set_scrollbar_mode(s_home, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(s_home, LV_OBJ_FLAG_SCROLLABLE);

  // ── home page layout (320x240) ───────────────────────────────────────────
  // y 1..26   agent name (font 20)
  // y 27..44  rank line (font 14, rank colour)
  // y 47..55  EXP bar + EXP value
  // y 62..167 mascot | speech bubble (62..140)
  // y 186..214 HP / BR bars
  // y 221..238 hint
  s_agentLbl = lv_label_create(s_home);
  lv_obj_set_pos(s_agentLbl, 10, 1);
  lv_obj_set_style_text_font(s_agentLbl, &lv_font_montserrat_20, 0);
  lv_label_set_text(s_agentLbl, "agent");

  s_rankLbl = lv_label_create(s_home);
  lv_obj_set_pos(s_rankLbl, 10, 27);
  lv_obj_set_width(s_rankLbl, 200);
  lv_label_set_long_mode(s_rankLbl, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(s_rankLbl, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_rankLbl, "…");

  s_expBar = lv_bar_create(s_home);
  lv_obj_set_size(s_expBar, 200, 8);
  lv_obj_set_pos(s_expBar, 10, 47);
  lv_bar_set_range(s_expBar, 0, 100);

  s_expLbl = lv_label_create(s_home);
  lv_obj_set_pos(s_expLbl, 218, 44);
  lv_obj_set_style_text_font(s_expLbl, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_expLbl, "EXP 0");

  // mascot (photo if boykisser selected, badge otherwise)
  const Mascot& mascot = Mascot::current();
  s_mascotX = 12;
  int mascotY = 62;
  s_mascot = lv_img_create(s_home);
  lv_img_set_src(s_mascot, &s_imgOpen);
  lv_obj_set_pos(s_mascot, s_mascotX, mascotY);
  s_mascotAlt = lv_obj_create(s_home);
  lv_obj_set_size(s_mascotAlt, 75, 75);
  lv_obj_set_pos(s_mascotAlt, s_mascotX, mascotY + 15);
  lv_obj_set_style_radius(s_mascotAlt, 37, 0);
  lv_obj_set_style_bg_opa(s_mascotAlt, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_mascotAlt, _c565(Config.getThemeColor()), 0);
  lv_obj_set_style_border_width(s_mascotAlt, 2, 0);
  lv_obj_t* altLbl = lv_label_create(s_mascotAlt);
  lv_label_set_text(altLbl, mascot.label);
  lv_obj_center(altLbl);
  if (String(mascot.id) == "boykisser")
    lv_obj_add_flag(s_mascotAlt, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(s_mascot, LV_OBJ_FLAG_HIDDEN);

  // speech bubble
  s_bubblePanel = lv_obj_create(s_home);
  lv_obj_add_style(s_bubblePanel, &s_styleBubble, 0);
  lv_obj_set_size(s_bubblePanel, 215, 78);
  lv_obj_set_pos(s_bubblePanel, 95, 62);
  lv_obj_clear_flag(s_bubblePanel, LV_OBJ_FLAG_SCROLLABLE);
  s_bubbleLbl = lv_label_create(s_bubblePanel);
  lv_obj_set_width(s_bubbleLbl, 195);
  lv_label_set_long_mode(s_bubbleLbl, LV_LABEL_LONG_WRAP);
  lv_label_set_text(s_bubbleLbl, "");
  lv_obj_align(s_bubbleLbl, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_color(s_bubbleLbl, _accent(), 0);
  lv_obj_set_style_text_font(s_bubbleLbl, &lv_font_montserrat_20, 0);

  // bottom: HP / BRAIN bars
  int barW = (W - 34) / 2;
  s_hpLbl = lv_label_create(s_home);
  lv_obj_set_pos(s_hpLbl, 10, H - 54);
  lv_obj_set_style_text_font(s_hpLbl, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_hpLbl, "HP 100%");

  s_brLbl = lv_label_create(s_home);
  lv_obj_set_pos(s_brLbl, 24 + barW, H - 54);
  lv_obj_set_style_text_font(s_brLbl, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_brLbl, "BR 0%");

  s_hpBar = lv_bar_create(s_home);
  lv_obj_set_size(s_hpBar, barW, 10);
  lv_obj_set_pos(s_hpBar, 10, H - 36);
  lv_bar_set_range(s_hpBar, 0, 100);
  lv_obj_set_style_bg_color(s_hpBar, lv_color_hex(0x202020), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_hpBar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);

  s_brBar = lv_bar_create(s_home);
  lv_obj_set_size(s_brBar, barW, 10);
  lv_obj_set_pos(s_brBar, 24 + barW, H - 36);
  lv_bar_set_range(s_brBar, 0, 100);
  lv_obj_set_style_bg_color(s_brBar, lv_color_hex(0x202020), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_brBar, _accent(), LV_PART_INDICATOR);

  s_hintLbl = lv_label_create(s_home);
  lv_obj_align(s_hintLbl, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_obj_set_style_text_font(s_hintLbl, &lv_font_montserrat_14, 0);
  lv_label_set_text(s_hintLbl, "PRESS " LV_SYMBOL_RIGHT " menu");
  lv_obj_set_style_text_color(s_hintLbl, lv_color_hex(0x707070), 0);

  // ── menu page ───────────────────────────────────────────────────────────
  s_menu = lv_obj_create(scr);
  lv_obj_set_size(s_menu, W, H);
  lv_obj_set_pos(s_menu, 0, 0);
  lv_obj_set_style_bg_color(s_menu, lv_color_black(), 0);
  lv_obj_set_style_border_width(s_menu, 0, 0);
  lv_obj_set_style_pad_all(s_menu, 0, 0);
  lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_menu, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);

  s_menuTitle = lv_label_create(s_menu);
  lv_obj_set_pos(s_menuTitle, 12, 8);
  lv_obj_set_style_text_font(s_menuTitle, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(s_menuTitle, _accent(), 0);
  {
    char t[40];
    snprintf(t, sizeof(t), "UniGeek %s", FIRMWARE_VERSION);
    lv_label_set_text(s_menuTitle, t);
  }

  s_menuList = lv_obj_create(s_menu);
  lv_obj_set_pos(s_menuList, 6, 36);
  lv_obj_set_size(s_menuList, W - 12, H - 42);
  lv_obj_set_style_bg_color(s_menuList, lv_color_black(), 0);
  lv_obj_set_style_border_width(s_menuList, 0, 0);
  lv_obj_set_style_pad_all(s_menuList, 4, 0);
  lv_obj_set_flex_flow(s_menuList, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(s_menuList, 6, 0);
  lv_obj_set_style_pad_column(s_menuList, 8, 0);
  // 8 entries x 2 columns = 4 rows x 40px + gaps fits H-42 with room, so the
  // list never needs to scroll (a scrollbar/scroll animation here showed up
  // as coloured side stripes + flicker when moving focus).
  lv_obj_clear_flag(s_menuList, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_menuList, LV_SCROLLBAR_MODE_OFF);

  struct Entry { const char* label; };
  const Entry entries[] = {
    { "Wifi" }, { "Bluetooth" }, { "HID" }, { "Modules" },
    { "Utilities" },
#if SHELL_MENU_LUA
    { "LUA" },
#endif
#if SHELL_MENU_GAMES
    { "Games" },
#endif
    { "Settings" },
  };
  s_menuCount = (int)(sizeof(entries) / sizeof(entries[0]));
  const int btnW = (W - 24 - 8) / 2;
  for (int i = 0; i < s_menuCount; i++) {
    lv_obj_t* b = lv_btn_create(s_menuList);
    lv_obj_set_size(b, btnW, 40);
    _styleBtns(b);
    lv_obj_set_style_text_font(b, &lv_font_montserrat_20, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, entries[i].label);
    lv_obj_center(l);
    s_menuBtns[i] = b;
  }
  _menuFocus(0);

  // ── periodic work ───────────────────────────────────────────────────────
  lv_timer_create(_infoTimer, 5000, nullptr);
  lv_timer_create(_blinkTimer, 150, nullptr);
  lv_timer_create(_bubbleTimer, 65, nullptr);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_mascot);
  lv_anim_set_exec_cb(&a, _swayExec);
  lv_anim_set_values(&a, 0, 11);
  lv_anim_set_time(&a, 5040);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);

  _refreshInfo();
  s_lvInit = true;
}

void LvglShellScreen::init() {
  _ensureUi();
  _showHome();
}

void LvglShellScreen::_showHome() {
  lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s_home, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(lv_scr_act());
}

void LvglShellScreen::_showMenu() {
  lv_obj_add_flag(s_home, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(lv_scr_act());
}

void LvglShellScreen::render() {
  if (Uni.lcdOff) return;
  _ensureUi();
  // No full-screen invalidate here: render() runs every loop(), and at the
  // 4 MHz SPI bus a whole-screen redraw takes ~300 ms — repainting the entire
  // screen each cycle caused visible tearing. LVGL tracks its own dirty
  // regions (timers/anims); we just pump it.
  lv_timer_handler();
}

void LvglShellScreen::onRestore() {
  // back from a legacy screen: redraw everything (it drew over the panel)
  lv_obj_invalidate(lv_scr_act());
}

void LvglShellScreen::update() {
  _ensureUi();

  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    const bool inMenu = !lv_obj_has_flag(s_menu, LV_OBJ_FLAG_HIDDEN);

    if (!inMenu) {
      if (dir == INavigation::DIR_PRESS || dir == INavigation::DIR_BACK)
        _showMenu();
    } else {
      switch (dir) {
        case INavigation::DIR_UP:
        case INavigation::DIR_LEFT:
          if (Uni.Speaker) Uni.Speaker->beep();
          _menuFocus(s_menuSel == 0 ? s_menuCount - 1 : s_menuSel - 1);
          break;
        case INavigation::DIR_DOWN:
        case INavigation::DIR_RIGHT:
          if (Uni.Speaker) Uni.Speaker->beep();
          _menuFocus(s_menuSel >= s_menuCount - 1 ? 0 : s_menuSel + 1);
          break;
        case INavigation::DIR_PRESS:
          _selectMenuItem(s_menuSel);
          break;
        case INavigation::DIR_BACK:
          _showHome();
          break;
        default: break;
      }
    }
  }

  lv_timer_handler();
}

void LvglShellScreen::_selectMenuItem(int index) {
  // Order mirrors the entries table in _ensureUi(); slots shift with the
  // same macros that drive MainMenuScreen's grid.
#if SHELL_MENU_LUA
  constexpr int kLuaIdx = 5;
#endif
#if SHELL_MENU_GAMES
  constexpr int kGamesIdx = 5 + SHELL_MENU_LUA;
#endif
  const int kSettingsIdx = 5 + SHELL_MENU_LUA + SHELL_MENU_GAMES;

  if (index == kSettingsIdx) { Screen.push(new SettingScreen()); return; }
  switch (index) {
    case 0: Screen.push(new WifiMenuScreen());     break;
    case 1: Screen.push(new BLEMenuScreen());      break;
    case 2: Screen.push(new KeyboardMenuScreen()); break;
    case 3: Screen.push(new ModuleMenuScreen());   break;
    case 4: Screen.push(new UtilityMenuScreen());  break;
#if SHELL_MENU_LUA
    case kLuaIdx: Screen.push(new LuaScreen());    break;
#endif
#if SHELL_MENU_GAMES
    case kGamesIdx: Screen.push(new GameMenuScreen()); break;
#endif
    default: break;
  }
}

#endif // USE_LVGL_SHELL
