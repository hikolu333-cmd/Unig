#include "BLESpamRunScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/AchievementManager.h"
#include "ui/components/TempBadge.h"

BLESpamRunScreen::~BLESpamRunScreen()
{
  _spam.end();
}

void BLESpamRunScreen::onInit()
{
  int n = Achievement.inc("ble_spam_first");
  if (n == 1) Achievement.unlock("ble_spam_first");

  _spam.begin((BleSpamUtil::Attack)_attack, _device);
  _lastTickMs = _lastDrawMs = millis();
}

void BLESpamRunScreen::onUpdate()
{
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _spam.end();
      Screen.goBack();
      return;
    }
  }

  uint32_t now = millis();
  _spam.tick();  // engine self-times via advMs/gapMs — call every loop
  if (now - _lastDrawMs >= 1000) {
    _lastDrawMs = now;
    _spinIdx = (_spinIdx + 1) % 4;
    render();
  }
}

void BLESpamRunScreen::onRender()
{
#ifdef USE_LVGL_SHELL
  if (!_chromeDrawn) {
    lv_obj_t* root = LvglUi::viewParent();

    _lblAttack = lv_label_create(root);
    lv_obj_set_style_text_font(_lblAttack, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblAttack, LvglUi::c565(TFT_GREEN), 0);
    lv_obj_set_pos(_lblAttack, 4, 4);
    lv_label_set_text(_lblAttack, BleSpamUtil::attackLabel(_attack));

    _lblDev = lv_label_create(root);
    lv_obj_set_style_text_font(_lblDev, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblDev, LvglUi::c565(TFT_DARKGREY), 0);
    lv_obj_set_pos(_lblDev, 4, 4 + bodyH() / 6);
    const char* devName =
      (_device >= 0 && _device < BleSpamUtil::deviceCount(_attack))
        ? BleSpamUtil::deviceLabel(_attack, _device) : "Random / All";
    lv_label_set_text(_lblDev, devName);

    _lblCfg = lv_label_create(root);
    lv_obj_set_style_text_font(_lblCfg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblCfg, LvglUi::c565(TFT_DARKGREY), 0);
    lv_obj_set_pos(_lblCfg, 4, 4 + bodyH() / 6 * 2);
    char cfg[40];
    snprintf(cfg, sizeof(cfg), "TX:%s  MAC:%s",
             BleSpamUtil::txLabel(BleSpamUtil::txPower),
             BleSpamUtil::macLabel(BleSpamUtil::macRand));
    lv_label_set_text(_lblCfg, cfg);

    _lblRun = lv_label_create(root);
    lv_obj_set_style_text_font(_lblRun, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblRun, LvglUi::c565(TFT_YELLOW), 0);
    lv_obj_set_pos(_lblRun, 4, 4 + bodyH() / 6 * 3);

    _chromeDrawn = true;
  }

  char line[32];
  snprintf(line, sizeof(line), "[%c] Spamming  Packets: %lu",
           _spinner[_spinIdx], (unsigned long)_spam.packets());
  lv_label_set_text(_lblRun, line);
  TempBadge::drawLv();
#else
  auto& lcd = Uni.Lcd;
  int bx = bodyX(), by = bodyY(), bw = bodyW(), bh = bodyH();

  if (!_chromeDrawn) {
    lcd.fillRect(bx, by, bw, bh, TFT_BLACK);
    lcd.setTextSize(1);
    lcd.setTextDatum(TL_DATUM);
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.drawString(BleSpamUtil::attackLabel(_attack), bx + 4, by + 4, 1);

    lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    const char* devName =
      (_device >= 0 && _device < BleSpamUtil::deviceCount(_attack))
        ? BleSpamUtil::deviceLabel(_attack, _device) : "Random / All";
    lcd.drawString(devName, bx + 4, by + 4 + bh / 6, 1);

    char cfg[40];
    snprintf(cfg, sizeof(cfg), "TX:%s  MAC:%s",
             BleSpamUtil::txLabel(BleSpamUtil::txPower),
             BleSpamUtil::macLabel(BleSpamUtil::macRand));
    lcd.drawString(cfg, bx + 4, by + 4 + bh / 6 * 2, 1);

    _chromeDrawn = true;
  }

  // Dynamic: spinner + packet count
  Sprite sp(&lcd);
  sp.createSprite(bw - 8, 14);
  sp.fillSprite(TFT_BLACK);
  sp.setTextDatum(TL_DATUM);
  sp.setTextColor(TFT_YELLOW, TFT_BLACK);
  char line[32];
  snprintf(line, sizeof(line), "[%c] Spamming  Packets: %lu",
           _spinner[_spinIdx], (unsigned long)_spam.packets());
  sp.drawString(line, 2, 0, 1);
  sp.pushSprite(bx + 4, by + 4 + bh / 6 * 3);
  sp.deleteSprite();
#endif
}
