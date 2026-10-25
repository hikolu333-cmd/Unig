//
// DIY RF Kit — Device factory.
//
// Display: HSPI (MOSI=11, MISO=13, SCK=12, CS=10, DC=9, RST=8) — ILI9341 + XPT2046 touch
// Radio:   CC1101 (CS=4, GDO0=5) + NRF24L01 (CE=7, CSN=16) — same HSPI bus, unique CS
// IR:      KY-005 LED on RMT TX (GPIO38), no RX module in this kit
// Input:   KY-023 analog joystick (VRx=1, VRy=2, SW=15) — see Navigation.h
// Storage: LittleFS only (no SD wired on this build)
//

#include "core/Device.h"
#include "Navigation.h"
#include "Display.h"
#include "Power.h"
#include <Wire.h>

static DisplayImpl    display;
static NavigationImpl navigation;
static PowerImpl      power;
static ExtSpiClass    sharedSpi(HSPI);  // TFT + touch + CC1101 + NRF24 share SCK/MOSI/MISO

void Device::boardHook() {}

Device* Device::createInstance() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // Assert every CS on the shared bus high before SPI init — avoids bus
  // contention on first transaction (lesson from reaper/t_embed_cc1101: an
  // unasserted CS floating low during another device's transfer corrupts it).
  const uint8_t spi_cs_pins[] = { LCD_CS, TOUCH_CS, CC1101_CS_PIN, NRF24_CSN_PIN };
  for (auto pin : spi_cs_pins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }

  Wire.begin(GROVE_SDA, GROVE_SCL);  // Grove I2C (ExI2C) — free for add-ons
  sharedSpi.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);

  auto* dev = new Device(display, power, &navigation, nullptr, &sharedSpi, nullptr);
  dev->ExI2C = &Wire;
  return dev;
}
