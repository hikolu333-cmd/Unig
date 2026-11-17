#include "MassStorageScreen.h"

#ifdef DEVICE_HAS_USB_HID

#include "core/Device.h"
#include "core/ScreenManager.h"
#include "utils/usb/UsbMscUtil.h"

namespace {
// Human-readable size into `out` (e.g. "29.7 GB", "512 MB").
void fmtSize(uint64_t bytes, char* out, size_t n)
{
  const char* unit = "B";
  double v = (double)bytes;
  if (v >= 1024.0) { v /= 1024.0; unit = "KB"; }
  if (v >= 1024.0) { v /= 1024.0; unit = "MB"; }
  if (v >= 1024.0) { v /= 1024.0; unit = "GB"; }
  if (v >= 100.0) snprintf(out, n, "%.0f %s", v, unit);
  else            snprintf(out, n, "%.1f %s", v, unit);
}
}  // namespace

void MassStorageScreen::onInit()
{
  if (!Uni.Storage || !Uni.Storage->isBlockDevice()) {
    _state = ST_UNSUPPORTED;
    return;
  }

  if (!UsbMscUtil::instance().begin(Uni.Storage)) {
    _state = (UsbMscUtil::instance().failReason() == UsbMscUtil::FAIL_USB_BUSY)
                 ? ST_USB_BUSY
                 : ST_UNSUPPORTED;
    return;
  }

  _state = ST_MOUNTED;
}

void MassStorageScreen::onUpdate()
{
  if (_state != ST_MOUNTED) {
    if (Uni.Nav->wasPressed()) {
      auto dir = Uni.Nav->readDirection();
      if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) Screen.goBack();
    }
    return;
  }

  auto& msc = UsbMscUtil::instance();

  // Redraw counters / eject status only when something changed.
  if (msc.sectorReads() != _lastReads || msc.sectorWrites() != _lastWrites ||
      msc.hostEjected() != _lastEjected) {
    render();
  }

  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) _exit();
  }
}

void MassStorageScreen::onRender()
{
#ifdef USE_LVGL_SHELL
  if (_state == ST_UNSUPPORTED) {
    _lvLine(0, "SD card not available", TFT_RED, -22, true);
    _lvLine(1, "Internal flash can't be", TFT_LIGHTGREY, 2, false);
    _lvLine(2, "exposed as a USB drive.", TFT_LIGHTGREY, 16, false);
    return;
  }
  if (_state == ST_USB_BUSY) {
    _lvLine(0, "USB busy", TFT_RED, -22, true);
    _lvLine(1, "Another USB feature", TFT_LIGHTGREY, 2, false);
    _lvLine(2, "claimed USB this boot.", TFT_LIGHTGREY, 16, false);
    _lvLine(3, "Reboot, open Mass Storage first.", TFT_YELLOW, 34, false);
    return;
  }

  auto& msc = UsbMscUtil::instance();
  const bool ejected = msc.hostEjected();

  if (!_chromeDrawn) {
    _lvLine(0, ejected ? "Ejected" : "Mounted",
            ejected ? TFT_ORANGE : TFT_GREEN, -26, true);
    _lvLine(1, "", TFT_LIGHTGREY, -4, false);
    _lvLine(2, "", TFT_DARKGREY, 12, false);
    _lblF = lv_label_create(LvglUi::viewParent());
    lv_obj_set_style_text_font(_lblF, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblF, LvglUi::c565(TFT_DARKGREY), 0);
    lv_obj_align(_lblF, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_label_set_text(_lblF, "[Back] Eject");
    _chromeDrawn = true;
  }
  _lastReads   = msc.sectorReads();
  _lastWrites  = msc.sectorWrites();
  _lastEjected = ejected;

  char buf[40];
  fmtSize(msc.capacityBytes(), buf, sizeof(buf));
  lv_obj_set_style_text_color(_lbl[0], LvglUi::c565(ejected ? TFT_ORANGE : TFT_GREEN), 0);
  lv_label_set_text(_lbl[0], ejected ? "Ejected" : "Mounted");
  lv_label_set_text(_lbl[1], buf);
  snprintf(buf, sizeof(buf), "R: %lu  W: %lu",
           (unsigned long)msc.sectorReads(), (unsigned long)msc.sectorWrites());
  lv_label_set_text(_lbl[2], buf);
#else
  auto& lcd = Uni.Lcd;
  const int cx = bodyX() + bodyW() / 2;

  if (_state == ST_UNSUPPORTED) {
    lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextSize(2);
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.drawString("SD card not available", cx, bodyY() + bodyH() / 2 - 16);
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    lcd.drawString("Internal flash can't be", cx, bodyY() + bodyH() / 2 + 4);
    lcd.drawString("exposed as a USB drive.", cx, bodyY() + bodyH() / 2 + 16);
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    return;
  }

  if (_state == ST_USB_BUSY) {
    lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextSize(2);
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.drawString("USB busy", cx, bodyY() + bodyH() / 2 - 16);
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    lcd.drawString("Another USB feature", cx, bodyY() + bodyH() / 2 + 4);
    lcd.drawString("claimed USB this boot.", cx, bodyY() + bodyH() / 2 + 16);
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.drawString("Reboot, open Mass Storage first.", cx, bodyY() + bodyH() / 2 + 34);
    return;
  }

  auto& msc = UsbMscUtil::instance();
  const bool ejected = msc.hostEjected();

  static constexpr int FOOTER_H = 12;
  const int contentH = bodyH() - FOOTER_H - 1;

  if (!_chromeDrawn) {
    lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);
    const int footerY = bodyY() + bodyH() - FOOTER_H;
    lcd.drawFastHLine(bodyX(), footerY - 1, bodyW(), TFT_DARKGREY);
    lcd.setTextSize(1);
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.setTextDatum(BC_DATUM);
    lcd.drawString("[Back] Eject", cx, footerY + 7);
    _chromeDrawn = true;
  }
  _lastReads   = msc.sectorReads();
  _lastWrites  = msc.sectorWrites();
  _lastEjected = ejected;

  // Body redrawn into a sprite each refresh so counters update flicker-free.
  const int spH = 70;
  Sprite sp(&lcd);
  sp.createSprite(bodyW(), spH);
  sp.fillSprite(TFT_BLACK);
  sp.setTextDatum(MC_DATUM);

  sp.setTextSize(2);
  sp.setTextColor(ejected ? TFT_ORANGE : TFT_GREEN, TFT_BLACK);
  sp.drawString(ejected ? "Ejected" : "Mounted", bodyW() / 2, 12);

  char buf[40];
  fmtSize(msc.capacityBytes(), buf, sizeof(buf));
  sp.setTextSize(1);
  sp.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  sp.drawString(buf, bodyW() / 2, 36);

  snprintf(buf, sizeof(buf), "R: %lu  W: %lu",
           (unsigned long)msc.sectorReads(), (unsigned long)msc.sectorWrites());
  sp.setTextColor(TFT_DARKGREY, TFT_BLACK);
  sp.drawString(buf, bodyW() / 2, 54);

  sp.pushSprite(bodyX(), bodyY() + (contentH - spH) / 2);
  sp.deleteSprite();
#endif
}

#ifdef USE_LVGL_SHELL
lv_obj_t* MassStorageScreen::_lvLine(int slot, const char* text, uint16_t color, int y, bool big)
{
  if (slot < 0 || slot > 3) return nullptr;
  if (!_lbl[slot]) _lbl[slot] = lv_label_create(LvglUi::viewParent());

  lv_obj_set_style_text_font(_lbl[slot], big ? &lv_font_montserrat_20 : &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_lbl[slot], LvglUi::c565(color), 0);
  lv_obj_align(_lbl[slot], LV_ALIGN_CENTER, 0, y);
  lv_label_set_text(_lbl[slot], text);
  return _lbl[slot];
}
#endif

void MassStorageScreen::_exit()
{
  UsbMscUtil::instance().end();
  Screen.goBack();
}

#endif  // DEVICE_HAS_USB_HID
