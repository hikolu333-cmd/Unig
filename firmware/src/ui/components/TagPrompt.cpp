#include "ui/components/TagPrompt.h"
#include "ui/components/Header.h"
#include "ui/components/StatusBar.h"
#include "core/Device.h"
#ifdef USE_LVGL_SHELL
#include "ui/lvgl/LvglUi.h"
#endif

#ifdef USE_LVGL_SHELL
// Full-body opaque "place tag" layer. Kept in a static so repeated show()
// calls update text in place; clear() removes it when the screen draws real
// content. If the owning page root was already deleted (screen change),
// lv_obj_is_valid fails and the prompt is rebuilt fresh on the new root.
namespace {
  lv_obj_t* _layer = nullptr;

  void _dropLayer() {
    if (_layer && lv_obj_is_valid(_layer)) lv_obj_del(_layer);
    _layer = nullptr;
  }
}

void TagPrompt::clear() { _dropLayer(); }

void TagPrompt::show(const char* message, int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* title)
{
  (void)0;  // `title` doubles as the optional yellow control hint line below
  LvglUi::ensure();
  lv_obj_t* root = LvglUi::viewParent();
  if (_layer && (!lv_obj_is_valid(_layer) || lv_obj_get_parent(_layer) != root)) {
    _dropLayer();
  }

  if (!_layer) {
    _layer = lv_obj_create(root);
    lv_obj_set_pos(_layer, x, y - LvglUi::viewOffsetY());
    lv_obj_set_size(_layer, w, h);
    lv_obj_set_style_bg_color(_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_layer, 0, 0);
    lv_obj_set_style_radius(_layer, 0, 0);
    lv_obj_set_style_pad_all(_layer, 0, 0);
    lv_obj_clear_flag(_layer, LV_OBJ_FLAG_SCROLLABLE);
  } else {
    lv_obj_set_pos(_layer, x, y - LvglUi::viewOffsetY());
    lv_obj_set_size(_layer, w, h);
    lv_obj_clear_flag(_layer, LV_OBJ_FLAG_HIDDEN);
  }

  if (message) {
    lv_obj_t* l = lv_label_create(_layer);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_center(l);
    lv_label_set_text(l, message);

    // Optional control hint below the main message (yellow, small).
    if (title) {
      lv_obj_t* hint = lv_label_create(_layer);
      lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(hint, LvglUi::c565(TFT_YELLOW), 0);
      lv_obj_align(hint, LV_ALIGN_CENTER, 0, 14);
      lv_label_set_text(hint, title);
    }
  }
}
#else // ── legacy TFT mode ───────────────────────────────────────────────────

void TagPrompt::clear() {}

void TagPrompt::show(const char* message, int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* title)
{
  Header header;
  if (title) header.render(title);
  StatusBar::refresh();

  auto& lcd = Uni.Lcd;
  lcd.fillRect(x, y, w, h, TFT_BLACK);

  if (message) {
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setTextDatum(MC_DATUM);
    lcd.drawString(message, x + w / 2, y + h / 2);
    lcd.setTextDatum(TL_DATUM);
  }
}
#endif
