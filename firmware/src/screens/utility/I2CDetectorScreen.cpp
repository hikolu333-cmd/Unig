#include "I2CDetectorScreen.h"
#include "core/Device.h"
#include "core/INavigation.h"
#include "core/ScreenManager.h"
#include "core/PinConfigManager.h"
#include "core/AchievementManager.h"
#include "screens/utility/UtilityMenuScreen.h"

void I2CDetectorScreen::onInit() {
  _hasBoth = (Uni.ExI2C != nullptr) && (Uni.InI2C != nullptr);

  // If only InI2C exists (no ExI2C), start on internal
  if (!Uni.ExI2C && Uni.InI2C) _wireIndex = 1;

  _scanning = true;
}

void I2CDetectorScreen::_scan() {
  memset(_found, 0, sizeof(_found));

  TwoWire* bus = (_wireIndex == 0) ? Uni.ExI2C : Uni.InI2C;
  if (!bus) return;

  if (_wireIndex == 0) {
    // ExI2C — always re-begin with the currently-configured pins so runtime
    // ext_sda/ext_scl changes from Settings → Pin apply without a reboot.
    int sda = PinConfig.getInt(PIN_CONFIG_EXT_SDA, PIN_CONFIG_EXT_SDA_DEFAULT);
    int scl = PinConfig.getInt(PIN_CONFIG_EXT_SCL, PIN_CONFIG_EXT_SCL_DEFAULT);
    bus->begin(sda, scl);
    bus->setTimeOut(50);
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
      bus->beginTransmission(addr);
      if (bus->endTransmission() == 0) _found[addr] = true;
    }
    bus->end();
  } else {
    // InI2C — managed by the board (PMIC, RTC, encoder, audio codec, …).
    // Never begin()/end() it; just scan in place.
    bus->setTimeOut(50);
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
      bus->beginTransmission(addr);
      if (bus->endTransmission() == 0) _found[addr] = true;
    }
  }

  int n = Achievement.inc("i2c_scan_first");
  if (n == 1) Achievement.unlock("i2c_scan_first");

  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    if (_found[addr]) { Achievement.unlock("i2c_device_found"); break; }
  }
}

void I2CDetectorScreen::onUpdate() {
  if (_scanning) {
    _scan();
    _scanning = false;
    render();
    return;
  }

  if (!Uni.Nav->wasPressed()) return;
  auto dir = Uni.Nav->readDirection();
  if (_hasBoth && (dir == INavigation::DIR_UP || dir == INavigation::DIR_DOWN)) {
    _wireIndex ^= 1;
    _scanning = true;
    render();
  } else if (dir == INavigation::DIR_PRESS || dir == INavigation::DIR_BACK) {
    Screen.goBack();
  }
}

void I2CDetectorScreen::onRender() {
#ifdef USE_LVGL_SHELL
  if (!_grid) _buildLv();

  // Bus label row
  char label[32];
  if (_hasBoth) {
    if (_wireIndex == 0) {
      int sda = PinConfig.getInt(PIN_CONFIG_EXT_SDA, PIN_CONFIG_EXT_SDA_DEFAULT);
      int scl = PinConfig.getInt(PIN_CONFIG_EXT_SCL, PIN_CONFIG_EXT_SCL_DEFAULT);
      snprintf(label, sizeof(label), "External (SDA:%d SCL:%d)", sda, scl);
    } else {
      snprintf(label, sizeof(label), "Internal");
    }
  } else {
    snprintf(label, sizeof(label), "I2C Bus");
  }
  lv_label_set_text(_lblBus, label);
  if (_hasBoth) lv_label_set_text(_lblFooter, "[Up/Dn] Bus");

  // Grid cell states
  const uint8_t cols = 16;
  const uint8_t rows = 8;
  for (uint8_t row = 0; row < rows; row++) {
    for (uint8_t col = 0; col < cols; col++) {
      uint8_t addr = (row << 4) | col;
      lv_obj_t* c = _cells[row * cols + col];
      if (!c) continue;
      bool found = (addr >= 0x08 && addr < 0x78) ? _found[addr] : false;
      if (found) {
        lv_obj_set_style_bg_color(c, LvglUi::c565(TFT_DARKGREEN), 0);
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X", addr);
        lv_label_set_text(_cellLbl[row * cols + col], buf);
      } else {
        lv_obj_set_style_bg_color(c, LvglUi::c565(0x2104), 0);
        lv_label_set_text(_cellLbl[row * cols + col], "");
      }
    }
  }

  lv_obj_set_style_bg_color(_lblScan, _scanning ? LvglUi::c565(TFT_BLACK) : lv_color_white(), 0);
#else
  auto& lcd = Uni.Lcd;
  lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);

  if (_scanning) {
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Scanning...", bodyX() + bodyW() / 2, bodyY() + bodyH() / 2);
    return;
  }

  // Bus label row
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setCursor(bodyX() + 2, bodyY() + 2);
  if (_hasBoth) {
    if (_wireIndex == 0) {
      int sda = PinConfig.getInt(PIN_CONFIG_EXT_SDA, PIN_CONFIG_EXT_SDA_DEFAULT);
      int scl = PinConfig.getInt(PIN_CONFIG_EXT_SCL, PIN_CONFIG_EXT_SCL_DEFAULT);
      char label[32];
      snprintf(label, sizeof(label), "External (SDA:%d SCL:%d)", sda, scl);
      lcd.print(label);
    } else {
      lcd.print("Internal");
    }
  } else {
    lcd.print("I2C Bus");
  }

  const uint16_t labelH  = 12;
  const uint16_t footerH = _hasBoth ? 12 : 0;
  const uint16_t gridY   = labelH + 4;
  const uint16_t gridH   = bodyH() - gridY - footerH - (footerH ? 5 : 4);
  const uint16_t gridW  = bodyW() - 2;

  const uint8_t cols = 16;
  const uint8_t rows = 8;
  const uint16_t cellW = gridW / cols;
  const uint16_t cellH = gridH / rows;

  // Draw grid cells
  for (uint8_t row = 0; row < rows; row++) {
    for (uint8_t col = 0; col < cols; col++) {
      uint8_t addr = (row << 4) | col;
      uint16_t x = 2 + col * cellW;
      uint16_t y = gridY + row * cellH;

      bool found = (addr >= 0x08 && addr < 0x78) ? _found[addr] : false;
      uint16_t bg = found ? TFT_DARKGREEN : 0x2104; // dark grey

      lcd.fillRect(bodyX() + x, bodyY() + y, cellW - 1, cellH - 1, bg);

      if (found) {
        lcd.setTextColor(TFT_WHITE, TFT_DARKGREEN);
        lcd.setTextSize(1);
        char buf[3];
        snprintf(buf, sizeof(buf), "%02X", addr);
        lcd.setCursor(bodyX() + x + 1, bodyY() + y + (cellH - 8) / 2);
        lcd.print(buf);
      }
    }
  }

  // Dedicated control footer only when bus switching is available.
  if (_hasBoth) {
    const int footerY = bodyY() + bodyH() - footerH;
    lcd.drawFastHLine(bodyX(), footerY - 1, bodyW(), TFT_DARKGREY);
    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setTextDatum(MC_DATUM);
    lcd.drawString("[Up/Dn] Bus", bodyX() + bodyW() / 2, footerY + 7);
  }
#endif
}

#ifdef USE_LVGL_SHELL
void I2CDetectorScreen::_buildLv() {
  LvglUi::ensure();
  lv_obj_t* root = LvglUi::viewParent();
  const int w = bodyW(), h = bodyH();

  _lblBus = lv_label_create(root);
  lv_obj_set_style_text_font(_lblBus, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_lblBus, LvglUi::c565(TFT_YELLOW), 0);
  lv_obj_set_pos(_lblBus, 2, 0);

  const uint16_t labelH  = 14;
  const uint16_t footerH = _hasBoth ? 14 : 0;
  const uint16_t gridY   = labelH + 4;
  const uint16_t gridH   = h - gridY - footerH - (footerH ? 5 : 4);
  const uint16_t gridW   = w - 2;

  const uint8_t cols = 16;
  const uint8_t rows = 8;
  const uint16_t cellW = gridW / cols;
  const uint16_t cellH = gridH / rows;

  _grid = lv_obj_create(root);
  lv_obj_set_pos(_grid, 0, gridY);
  lv_obj_set_size(_grid, w, gridH);
  lv_obj_set_style_bg_opa(_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_grid, 0, 0);
  lv_obj_set_style_pad_all(_grid, 0, 0);
  lv_obj_clear_flag(_grid, LV_OBJ_FLAG_SCROLLABLE);

  for (uint8_t row = 0; row < rows; row++) {
    for (uint8_t col = 0; col < cols; col++) {
      uint8_t addr = (row << 4) | col;
      const uint16_t x = 2 + col * cellW;
      const uint16_t y = row * cellH;
      const int idx = row * cols + col;

      lv_obj_t* c = lv_obj_create(_grid);
      lv_obj_set_pos(c, x, y);
      lv_obj_set_size(c, cellW - 1, cellH - 1);
      lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(c, 0, 0);
      lv_obj_set_style_border_width(c, 0, 0);
      lv_obj_set_style_pad_all(c, 0, 0);
      lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_bg_color(c, LvglUi::c565(0x2104), 0);

      lv_obj_t* l = lv_label_create(c);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, lv_color_white(), 0);
      lv_obj_center(l);
      if (addr < 0x08 || addr >= 0x78) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);

      _cells[idx]   = c;
      _cellLbl[idx] = l;
    }
  }

  _lblScan = lv_label_create(root);
  lv_obj_set_style_text_font(_lblScan, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_lblScan, LvglUi::c565(TFT_YELLOW), 0);
  lv_obj_align(_lblScan, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(_lblScan, "Scanning...");
}
#endif