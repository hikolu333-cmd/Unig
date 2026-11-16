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
#include "RadioKeeper.h"    // service-key 'r': weak-rail re-arm

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;

//
// Service console — radio re-arm key 'r' only.
//
// 'r' clears RadioKeeper's weak-rail failure state (see RadioKeeper.h): the
// next WiFi/BT screen retries the radio from scratch. Accepted on BOTH
// serial links — the native USB console and the UART0 bridge (the latter
// survives radio power-up). On the native USB side it is taken with peek()
// only, so bytes belonging to the USB File Manager / Remote protocols
// (ctx 'F'/'S', see UartFileManager) are never consumed here; UART0 input
// has no other consumer, so that link is drained freely.
//
void Device::boardHook() {
  while (Serial0.available() > 0) {
    if ((char)Serial0.read() == 'r') RadioKeeper::resetFails();
  }
  if (Serial.available() > 0 && Serial.peek() == 'r') {
    Serial.read();
    RadioKeeper::resetFails();
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
