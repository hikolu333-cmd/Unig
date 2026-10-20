//
// DIY Joystick (bare ESP32-S3 DevKit + 2.8" ILI9341 SPI TFT "Serial Port Module A"
// + KY-023 dual-axis analog joystick).
//
// Wiring:
//   Display (ILI9341, SPI):
//     CS=10  DC=9  RST=8  MOSI=11  SCLK=12  MISO=13  LED(BL)=14 (PWM)
//     Touch (XPT2046) pins on the module are present but left unconnected —
//     the joystick is the primary input, not the touch panel. TOUCH_CS is
//     disabled below; wire T_CLK/T_CS/T_DIN/T_DO/T_IRQ later if you want to
//     add touch support on top of the joystick.
//
//   KY-023 joystick:
//     VRx -> GPIO1  (ADC1_CH0)
//     VRy -> GPIO2  (ADC1_CH1)
//     SW  -> GPIO15 (digital, active LOW, internal pull-up)
//     Both VRx/VRy are on ADC1 (not ADC2), so reading them while WiFi is
//     active is safe.
//
//   Grove-style external I2C (free for NFC/GPS/sensor add-ons later):
//     SDA=17  SCL=18
//
// Flash/PSRAM assumed 16MB/8MB (common ESP32-S3-DevKitC-1 N16R8 module), same
// as diy_smoochie. If your module is smaller (N8R8/N4R2), adjust
// board_build.partitions in config.ini and the N16R8-specific flags in
// firmware/boards/_devices/diy_joystick.json.
//

#pragma once
#include <stdint.h>

// ─── Display SPI ──────────────────────────────────────────
#define TFT_MOSI_PIN  11
#define TFT_MISO_PIN  13
#define TFT_SCLK_PIN  12

static const uint8_t MOSI = TFT_MOSI_PIN;
static const uint8_t MISO = TFT_MISO_PIN;
static const uint8_t SCK  = TFT_SCLK_PIN;
static const uint8_t SS   = 10;

// ─── I2C (Grove — external modules) ───────────────────────
#define GROVE_SDA  17
#define GROVE_SCL  18

static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

// ─── LCD Backlight ────────────────────────────────────────
#define LCD_BL     14
#define LCD_BL_CH  7

// ─── KY-023 Joystick ──────────────────────────────────────
#define JOY_VRX_PIN  1    // ADC1_CH0
#define JOY_VRY_PIN  2    // ADC1_CH1
#define JOY_SW_PIN   15   // digital, active LOW (internal pull-up)

// ─── TFT_eSPI config ───────────────────────────────────────
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USER_SETUP_LOADED 1

#define ILI9341_2_DRIVER
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
#define TFT_MOSI    11
#define TFT_SCLK    12
#define TFT_MISO    13
#define TFT_CS      10
#define TFT_DC      9
#define TFT_RST     8
#define TFT_BL      -1          // backlight via LEDC (see LCD_BL)
#define TFT_BACKLIGHT_ON HIGH
#define TFT_DEFAULT_ORIENTATION  1
#define TOUCH_CS    -1          // touch not wired/used — joystick is primary nav
#define SMOOTH_FONT 1
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  16000000

// ─── App feature flags ─────────────────────────────────────
// Joystick gives genuine, independent UP/DOWN/LEFT/RIGHT (not aliased from
// 2 buttons), so enable row/column grid navigation in MainMenuScreen.
#define DEVICE_HAS_4WAY_NAV
