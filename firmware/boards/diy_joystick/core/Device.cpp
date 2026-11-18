//
// DIY Joystick — Device factory.
//
// Display: own SPI bus (MOSI=38, SCK=39, CS=21, DC=47, BL=40) — ILI9341
// RF bus:  HSPI SCK=12/MOSI=11/MISO=13 — CC1101 (CS=10, GDO0=9) + NRF24 N1
//          (CE=5, CSN=16); N2–N4 held deselected (see createInstance)
// IR:      KY-005 LED on GPIO14
// Input:   KY-023 analog joystick (VRx=1, VRy=2, SW=42) — see Navigation.h
// Storage: LittleFS only (no SD wired on this build)
//

#include "core/Device.h"
#include "Navigation.h"
#include "Display.h"
#include "Power.h"
#include "RadioKeeper.h"    // service-key 'r': weak-rail re-arm
#include <Wire.h>

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;
static ExtSpiClass    rfSpi(HSPI);   // CC1101: SCK=12/MOSI=11/MISO=13
static ExtSpiClass    nrfSpi(VSPI);  // NRF24: dedicated bus SCK=18/MOSI=17/MISO=16

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

  // RF bus housekeeping: assert every CS high before init — a floating CSN
  // during another device's transfer corrupts the bus. GPIO16–18 belong to
  // the dedicated NRF SPI bus now, so only real CE/CSN pads are touched.
  static const uint8_t rfCsPins[] = { CC1101_CS_PIN, 6, 15 };   // N1/N2 CSN
  for (auto pin : rfCsPins) { pinMode(pin, OUTPUT); digitalWrite(pin, HIGH); }
  static const uint8_t rfCePins[] = { 5, 7 };                    // N1/N2 CE
  for (auto pin : rfCePins) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }

  Wire.begin(GROVE_SDA, GROVE_SCL);  // Grove I2C (ExI2C) — free for add-ons
  rfSpi.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);
  nrfSpi.begin(NRF_SPI_SCK_PIN, NRF_SPI_MISO_PIN, NRF_SPI_MOSI_PIN, -1);

  auto* dev = new Device(display, power, &navigation, nullptr, &rfSpi, nullptr, &nrfSpi);
  dev->ExI2C = &Wire;
  return dev;
}
