#include "WifiESPNowChatScreen.h"
#include "core/Device.h"
#include "core/ScreenManager.h"
#include "core/ConfigManager.h"
#include "screens/wifi/WifiMenuScreen.h"
#include "ui/actions/InputTextAction.h"
#include "ui/actions/ShowStatusAction.h"

#include <WiFi.h>
#include <esp_now.h>

// ── Static definitions ─────────────────────────────────────────────────────

WifiESPNowChatScreen* WifiESPNowChatScreen::_instance  = nullptr;
const uint8_t         WifiESPNowChatScreen::_broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t         WifiESPNowChatScreen::_code[5]      = {0x00, 0x01, 0x14, 0x25, 0x36};

// ── Destructor ─────────────────────────────────────────────────────────────

WifiESPNowChatScreen::~WifiESPNowChatScreen()
{
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  _instance = nullptr;
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void WifiESPNowChatScreen::onInit()
{
  _instance = this;

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    ShowStatusAction::show("Failed to start ESP-NOW", 1500);
    Screen.goBack();
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, _broadcast, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (!esp_now_is_peer_exist(_broadcast)) {
    if (esp_now_add_peer(&peer) != ESP_OK) {
      ShowStatusAction::show("Failed to add peer", 1500);
      Screen.goBack();
      return;
    }
  }

  esp_now_register_recv_cb(&WifiESPNowChatScreen::onDataRecvCb);
}

void WifiESPNowChatScreen::onUpdate()
{
  if (_composing) return;

  if (_dirty) {
    _dirty = false;
    render();
  }

  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK) {
      Screen.goBack();
      return;
    }
    // DIR_PRESS = encoder button (no-keyboard) or ENTER key (keyboard boards)
    // NavigationImpl converts \n → DIR_PRESS before onUpdate() runs
    if (dir == INavigation::DIR_PRESS) {
      _composing = true;
      String msg = InputTextAction::popup("Message");
      render();
      _composing = false;
      if (msg.length() > 0) sendMessage(msg.c_str());
    }
  }
}

void WifiESPNowChatScreen::onRender()
{
  const uint16_t themeColor = Config.getThemeColor();
  const int      FOOTER_H   = 14;

#ifdef USE_LVGL_SHELL
  if (!_cont) {
    lv_obj_t* root = LvglUi::viewParent();

    _cont = lv_obj_create(root);
    lv_obj_set_pos(_cont, 0, 0);
    lv_obj_set_size(_cont, bodyW(), bodyH() - FOOTER_H);
    lv_obj_set_style_bg_opa(_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_cont, 0, 0);
    lv_obj_set_style_pad_all(_cont, 0, 0);
    lv_obj_set_style_pad_row(_cont, 0, 0);
    lv_obj_set_flex_flow(_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(_cont, LV_SCROLLBAR_MODE_OFF);

    _lblFooter = lv_label_create(root);
    lv_obj_set_style_text_font(_lblFooter, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblFooter, LvglUi::c565(TFT_DARKGREY), 0);
    lv_obj_align(_lblFooter, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_label_set_text(_lblFooter, "[Press] Msg");
  }

  // Rebuild the message column (≤20 lines — cheap, same pattern as LogView).
  lv_obj_clean(_cont);
  portENTER_CRITICAL(&_lock);
  for (int i = 0; i < _msgCount; i++) {
    const Entry& e = _messages[i];

    char name[15] = {};
    strncpy(name, e.msg.name, 14);
    char text[225] = {};
    strncpy(text, e.msg.text, 224);
    char line[64];
    snprintf(line, sizeof(line), "[%s] %s", name, text);

    // Sent messages use theme color, received use white
    bool sent = (e.sender[0] == 0xFF && e.sender[1] == 0xFF &&
                 e.sender[2] == 0xFF && e.sender[3] == 0xFF &&
                 e.sender[4] == 0xFF && e.sender[5] == 0xFF);
    lv_obj_t* l = lv_label_create(_cont);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, sent ? LvglUi::c565(themeColor) : lv_color_white(), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, line);
  }
  portEXIT_CRITICAL(&_lock);

  // Newest at the bottom: scroll to the end of the column.
  lv_obj_scroll_to_y(_cont, LV_COORD_MAX, LV_ANIM_OFF);
#else
  const int      LINE_H     = 9;

  auto& lcd = Uni.Lcd;
  lcd.fillRect(bodyX(), bodyY(), bodyW(), bodyH(), TFT_BLACK);

  // Dedicated control footer.
  const int footerY = bodyY() + bodyH() - FOOTER_H;
  lcd.drawFastHLine(bodyX(), footerY - 1, bodyW(), TFT_DARKGREY);
  lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextSize(1);
  lcd.drawString("[Press] Msg", bodyX() + bodyW() / 2, footerY + 7);

  // Messages: newest at bottom, oldest at top
  portENTER_CRITICAL(&_lock);
  int y = bodyH() - FOOTER_H - 2 - LINE_H;
  for (int i = _msgCount - 1; i >= 0 && y >= 0; i--) {
    const Entry& e = _messages[i];

    char name[15] = {};
    strncpy(name, e.msg.name, 14);

    char text[225] = {};
    strncpy(text, e.msg.text, 224);

    char line[64];
    snprintf(line, sizeof(line), "[%s] %s", name, text);

    // Sent messages use theme color, received use white
    bool sent = (e.sender[0] == 0xFF && e.sender[1] == 0xFF &&
                 e.sender[2] == 0xFF && e.sender[3] == 0xFF &&
                 e.sender[4] == 0xFF && e.sender[5] == 0xFF);
    lcd.setTextColor(sent ? themeColor : TFT_WHITE, TFT_BLACK);
    lcd.setTextDatum(TL_DATUM);
    lcd.drawString(line, bodyX(), bodyY() + y);
    y -= LINE_H;
  }
  portEXIT_CRITICAL(&_lock);
#endif
}

// ── ESP-NOW callbacks ──────────────────────────────────────────────────────

void WifiESPNowChatScreen::onDataRecv(const uint8_t* mac, const uint8_t* data, int len)
{
  if (len < (int)sizeof(Message)) return;

  const Message* msg = reinterpret_cast<const Message*>(data);
  if (memcmp(msg->code, _code, sizeof(_code)) != 0) return;

  portENTER_CRITICAL(&_lock);
  if (_msgCount >= MAX_MESSAGES) {
    // Shift out oldest
    for (int i = 0; i < MAX_MESSAGES - 1; i++) _messages[i] = _messages[i + 1];
    _msgCount = MAX_MESSAGES - 1;
  }
  memcpy(_messages[_msgCount].sender, mac, 6);
  memcpy(&_messages[_msgCount].msg, msg, sizeof(Message));
  _msgCount++;
  portEXIT_CRITICAL(&_lock);

  if (Uni.Speaker) Uni.Speaker->playNotification();

  // Auto-reply to ping
  if (msg->messageLength >= 5 && memcmp(msg->text, "!ping", 5) == 0) {
    sendMessage("!pong");
    return;
  }

  _dirty = true;
}

// ── Send ──────────────────────────────────────────────────────────────────

void WifiESPNowChatScreen::sendMessage(const char* text)
{
  String name  = Config.get(APP_CONFIG_DEVICE_NAME, APP_CONFIG_DEVICE_NAME_DEFAULT);
  size_t tlen  = strlen(text);
  if (tlen > 224) tlen = 224;

  Message msg = {};
  memcpy(msg.code, _code, sizeof(_code));
  msg.nameLength    = (uint8_t)name.length();
  msg.messageLength = (uint8_t)tlen;
  strncpy(msg.name, name.c_str(), sizeof(msg.name));
  strncpy(msg.text, text, sizeof(msg.text));

  esp_err_t res = esp_now_send(_broadcast,
                               reinterpret_cast<uint8_t*>(&msg),
                               sizeof(msg));
  if (res != ESP_OK) {
    ShowStatusAction::show("Failed", 1500);
    render();
    return;
  }

  // Add to own message queue as sent (sender = broadcast addr as convention)
  portENTER_CRITICAL(&_lock);
  if (_msgCount >= MAX_MESSAGES) {
    for (int i = 0; i < MAX_MESSAGES - 1; i++) _messages[i] = _messages[i + 1];
    _msgCount = MAX_MESSAGES - 1;
  }
  memcpy(_messages[_msgCount].sender, _broadcast, 6);
  memcpy(&_messages[_msgCount].msg, &msg, sizeof(Message));
  _msgCount++;
  portEXIT_CRITICAL(&_lock);

  render();
}
