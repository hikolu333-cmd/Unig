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

void Device::boardHook() {}

Device* Device::createInstance() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  Wire.begin(GROVE_SDA, GROVE_SCL);  // Grove I2C (ExI2C) — free for add-ons

  auto* dev = new Device(display, power, &navigation);
  dev->ExI2C = &Wire;
  return dev;
}
