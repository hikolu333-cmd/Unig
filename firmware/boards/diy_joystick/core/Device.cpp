//
// DIY Joystick — Device factory.
//
// Display: SPI (MOSI=11, MISO=13, SCK=12, CS=10, DC=9, RST=8) — ILI9341
// Input:   KY-023 analog joystick (VRx=1, VRy=2, SW=15) — see Navigation.h
// Storage: LittleFS only (no SD wired on this build)
//

#include "core/Device.h"
#include "Navigation.h"
#include "Display.h"
#include "Power.h"
#include "RadioKeeper.h"    // remote radio re-arm ('r')
#include <Preferences.h>   // remote radio re-arm ('r')

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;

//
// PC Remote — control the device from a PC over either serial link while the
// physical joystick is unwired. Keys are accepted on BOTH channels: the
// native USB console (USB-C) and UART0 (CH9102 bridge on the micro-USB port,
// which keeps working when the WiFi radio powers up). Single-character
// commands:
//   w/W/UP    → DIR_UP      s/S/DOWN → DIR_DOWN
//   a/A/LEFT  → DIR_LEFT    d/D/RIGHT→ DIR_RIGHT
//   e/Enter/Space → DIR_PRESS   b/B/Backspace → DIR_BACK
//   h/H       → long PRESS (hold, auto-released by serviceInject)
//   r         → radio re-arm (service key, see RadioKeeper)
// Nothing else is consumed, so the line stays free for logs/other traffic.
//
void Device::boardHook() {
  // PC Remote keys arrive on BOTH links: HWCDC (USB-C) and UART0 — the
  // CH9102 bridge on the micro-USB port survives radio power-up, the native
  // USB console often does not.
  while (true) {
    char c;
    if (Serial.available() > 0)       c = (char)Serial.read();
    else if (Serial0.available() > 0) c = (char)Serial0.read();
    else break;
    // 'r' = remote radio re-arm (service key): reset the warm-up failure
    // counter and retry the one-shot radio warm-up right now. On boards
    // whose rail is marginal this is a retry lottery; the boot-loop chain
    // continues automatically after any power-loss reset.
    if (c == 'r') {
      Serial0.println("[R] radio re-arm: clearing counters, warm-up now");
      {
        Preferences prefs;
        prefs.begin("radio", false);
        prefs.putBool("attempting", false);
        prefs.putUChar("fails", 0);
        prefs.end();
      }
      if (RadioKeeper::isUp()) {
        Serial0.println("[R] radio already up");
        continue;
      }
      // dark, sleeping panel first: the retry odds are better at minimal load
      ledcWrite(LCD_BL_CH, 0);
      pinMode(LCD_BL, OUTPUT);
      digitalWrite(LCD_BL, LOW);
      Lcd.writecommand(0x10);   // display sleep
      delay(300);
      RadioKeeper::warmUp();
      Lcd.writecommand(0x11);   // wake display
      digitalWrite(LCD_BL, HIGH);
      Serial0.printf("[R] warmUp returned, radio %s\n",
                     RadioKeeper::isUp() ? "UP" : "down");
      continue;
    }


    INavigation::Direction dir = INavigation::DIR_NONE;
    bool hold = false;
    switch (c) {
      case 'w': case 'W': case 'U': dir = INavigation::DIR_UP;    break;
      case 's': case 'S': case 'D': dir = INavigation::DIR_DOWN;  break;
      case 'a': case 'A': case 'L': dir = INavigation::DIR_LEFT;  break;
      case 'd': case 'R':           dir = INavigation::DIR_RIGHT; break;
      case 'e': case '\r': case '\n': dir = INavigation::DIR_PRESS; break;
      case 'b': case 'B': case 0x7F: dir = INavigation::DIR_BACK; break;
      case 'h': case 'H': dir = INavigation::DIR_PRESS; hold = true; break;
      default: continue;  // logs/unknown bytes — ignore
    }

    if (hold) Nav->injectHold(dir);
    else      Nav->inject(dir);
    lastActiveMs = millis();
  }
}

Device* Device::createInstance() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  Wire.begin(GROVE_SDA, GROVE_SCL);  // Grove I2C (ExI2C) — free for add-ons

  auto* dev = new Device(display, power, &navigation);
  dev->ExI2C = &Wire;
  return dev;
}
