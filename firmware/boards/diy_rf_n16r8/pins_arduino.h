//
// DIY RF Kit — ESP32-S3-N16R8 (16MB flash / 8MB PSRAM) + 2.8" ILI9341 SPI
// TFT "Serial Port Module A" (touch wired) + KY-023 joystick + CC1101 +
// NRF24L01 + KY-005 IR LED. TP4056/MT3606 are power-only, no GPIO.
//
// Shared HSPI bus (USE_HSPI_PORT): SCK=12 MISO=13 MOSI=11 — display, touch,
// CC1101 and NRF24 all sit on this bus with unique CS. Every CS is driven
// HIGH before SPI.begin() (see Device.cpp) so no device holds the bus at
// boot. GPIO 26-37 are QSPI/OPI flash+PSRAM on N16R8 — untouched below.
// GPIO 19/20 left free (native USB D-/D+, ARDUINO_USB_CDC_ON_BOOT=1).
//
// Wiring:
//   Display (ILI9341, HSPI): CS=10 DC=9 RST=8 BL=14(PWM)
//   Touch   (XPT2046, same bus): CS=6  (no IRQ wired — polled)
//   KY-023 joystick: VRx=1(ADC1_CH0) VRy=2(ADC1_CH1) SW=15 (active LOW, pull-up)
//   CC1101 (HSPI): CS=4  GDO0=5 (no antenna-switch SW0/SW1 on this module)
//   NRF24L01 (HSPI): CE=7  CSN=16
//   KY-005 IR LED (RMT TX, no RX module in this kit): TX=38
//   Grove-style external I2C (free for future add-ons): SDA=17 SCL=18
//

#pragma once
#include <stdint.h>

// ─── Shared SPI bus (HSPI) ─────────────────────────────────
#define USE_HSPI_PORT
#define SPI_MOSI_PIN  11
#define SPI_MISO_PIN  13
#define SPI_SCK_PIN   12

static const uint8_t MOSI = SPI_MOSI_PIN;
static const uint8_t MISO = SPI_MISO_PIN;
static const uint8_t SCK  = SPI_SCK_PIN;
static const uint8_t SS   = 10;   // TFT CS

// ─── I2C (Grove — external modules) ───────────────────────
#define GROVE_SDA  17
#define GROVE_SCL  18

static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

// ─── LCD Backlight ────────────────────────────────────────
#define LCD_CS     10
#define LCD_BL     14
#define LCD_BL_CH  7

// ─── KY-023 Joystick ──────────────────────────────────────
#define JOY_VRX_PIN  1    // ADC1_CH0
#define JOY_VRY_PIN  2    // ADC1_CH1
#define JOY_SW_PIN   15   // digital, active LOW (internal pull-up)

// ─── CC1101 Sub-GHz (shared HSPI bus) ─────────────────────
#define CC1101_CS_PIN    4
#define CC1101_GDO0_PIN  5
// No antenna-switch module on this kit — CC1101_SW0_PIN/SW1_PIN left undefined.

// ─── NRF24L01+ (shared HSPI bus) ───────────────────────────
#define NRF24_CE_PIN     7
#define NRF24_CSN_PIN    16

// ─── KY-005 IR LED (RMT TX only — no IR receiver in this kit) ─
#define IR_TX_PIN  38

// ─── TFT_eSPI config ───────────────────────────────────────
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USER_SETUP_LOADED 1

#define ILI9341_2_DRIVER
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
#define TFT_MOSI    SPI_MOSI_PIN
#define TFT_SCLK    SPI_SCK_PIN
#define TFT_MISO    SPI_MISO_PIN
#define TFT_CS      LCD_CS
#define TFT_DC      9
#define TFT_RST     8
#define TFT_BL      -1          // backlight via LEDC (see LCD_BL)
#define TFT_BACKLIGHT_ON HIGH
#define TFT_DEFAULT_ORIENTATION  1
#define TOUCH_CS    6           // XPT2046 on the same bus, polled (no IRQ pin wired)
#define SMOOTH_FONT 1
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY 2500000

// ─── App feature flags ─────────────────────────────────────
// Joystick gives genuine, independent UP/DOWN/LEFT/RIGHT (not aliased from
// 2 buttons), so enable row/column grid navigation in MainMenuScreen.
#define DEVICE_HAS_4WAY_NAV
