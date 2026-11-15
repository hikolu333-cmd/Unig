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

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;

//
// PC Remote — control the device from a PC over the USB CDC serial link
// while the physical joystick is unwired. Single-character commands:
//   w/W/UP    → DIR_UP      s/S/DOWN → DIR_DOWN
//   a/A/LEFT  → DIR_LEFT    d/D/RIGHT→ DIR_RIGHT
//   e/Enter/Space → DIR_PRESS   b/B/Backspace → DIR_BACK
//   h/H       → long PRESS (hold, auto-released by serviceInject)
// Nothing else is consumed, so the line stays free for logs/other traffic.
//
void Device::boardHook() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

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
