//
// DIY Joystick (bare ESP32-S3 DevKit + 2.8" ILI9341-family SPI TFT "Serial Port Module A"
// + KY-023 dual-axis analog joystick + RF kit).
//
// Wiring (solder map "финал", 2026-11):
//   Display (SPI, own bus — TFT_eSPI FSPI):
//     CS=21  DC=47  RST=EN платы (программный сброс, TFT_RST=-1)
//     MOSI=38  SCLK=39  MISO=-1 (SDO дисплея не подключён)
//     LED(BL)=40 (PWM через LEDC)
//     Touch (XPT2046): T_CS=41, T_DO=4, T_CLK/T_DIN — перемычки на SCK/MOSI
//
//   RF bus (HSPI, separate from the display):
//     SCK=12  MOSI=11  MISO=13
//     CC1101: CS=10  GDO0=9 (GDO2 не подключён, не нужен)
//     NRF24L01: модуль N1: CE=5  CSN=16  — прошивка умеет ОДИН модуль;
//       N2 (CE=6/CSN=17), N3 (CE=7/CSN=18), N4 (CE=15/CSN=8) физически на
//       шине — их CSN/CE удерживаются неактивными (Device factory).
//   IR: KY-005 LED, S=14 (RMT TX)
//
//   KY-023 joystick (VCC=3V3!):
//     VRx -> GPIO1  (ADC1_CH0)
//     VRy -> GPIO2  (ADC1_CH1)
//     SW  -> GPIO42 (digital, active LOW, internal pull-up)
//     Both VRx/VRy are on ADC1 (not ADC2), so reading them while WiFi is
//     active is safe.
//
//   Grove-style external I2C (free for NFC/GPS/sensor add-ons later):
//     SDA=3  SCL=45  — (17/18 отошли под NRF CSN; 45 подтянут вверх и так)
//
// Flash: 16MB (проверено esptool: Manufacturer 0x68, Device 0x4018).
// SPI-частота дисплея 16 МГц (см. ниже).
//

#pragma once
#include <stdint.h>

// ─── Display SPI ──────────────────────────────────────────
#define TFT_MOSI_PIN  38
#define TFT_MISO_PIN  -1
#define TFT_SCLK_PIN  39

static const uint8_t MOSI = TFT_MOSI_PIN;
static const uint8_t MISO = 4;   // T_DO тача (для чтений дисплея не используется)
static const uint8_t SCK  = TFT_SCLK_PIN;
static const uint8_t SS   = 21;  // TFT CS

// ─── I2C (Grove — external modules) ───────────────────────
// Was SDA=17/SCL=18 — those GPIOs are NRF2/NRF3 CSN on the solder map now.
#define GROVE_SDA  3
#define GROVE_SCL  45

static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

// ─── RF SPI bus (HSPI — CC1101 + NRF24L01) ────────────────
#define SPI_SCK_PIN   12
#define SPI_MISO_PIN  13
#define SPI_MOSI_PIN  11

// ─── CC1101 Sub-GHz (RF bus) ──────────────────────────────
#define CC1101_CS_PIN    10
#define CC1101_GDO0_PIN   9

// ─── NRF24L01+ (RF bus; firmware drives module N1, jam-all drives all 4) ──
#define NRF24_CE_PIN     5
#define NRF24_CSN_PIN   16
#define NRF24_COUNT      4
#define NRF24_CE_PINS    { 5, 6, 7, 15 }
#define NRF24_CSN_PINS   { 16, 17, 18, 8 }

// ─── IR (KY-005 LED) ──────────────────────────────────────
#define IR_TX_PIN  14

// ─── LCD Backlight ────────────────────────────────────────
#define LCD_BL     40
#define LCD_BL_CH  7

// ─── KY-023 Joystick ──────────────────────────────────────
#define JOY_VRX_PIN  1    // ADC1_CH0
#define JOY_VRY_PIN  2    // ADC1_CH1
#define JOY_SW_PIN   42   // digital, active LOW (internal pull-up)

// ─── TFT_eSPI config ───────────────────────────────────────
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USER_SETUP_LOADED 1

// ESP32-S3 + arduino core 2.x: default SPI_PORT=FSPI(0) makes IDF register
// macros (REG_SPI_BASE(0)=0 → write to 0x10 → StoreProhibited bootloop).
// USE_FSPI_PORT switches TFT_eSPI to SPI_PORT=2 = real SPI2 registers.
#define USE_FSPI_PORT

// Panel needs INVON — without it the background renders white.
#define TFT_INVERSION_ON

// KY-023 joystick is wired (solder map): VRx=1, VRy=2, SW=42, powered 3V3.
// (JOYSTICK_NOT_WIRED removed — floating-input workaround no longer needed.)

#define ILI9341_2_DRIVER
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
#define TFT_MOSI    38
#define TFT_SCLK    39
#define TFT_MISO    4            // T_DO тача — общий провод, дисплею не мешает
#define TFT_CS      21
#define TFT_DC      47
#define TFT_RST     -1           // RESET завязан на EN платы — сброс программный
#define TFT_BL      -1           // backlight via LEDC (see LCD_BL)
#define TFT_BACKLIGHT_ON HIGH
#define TFT_DEFAULT_ORIENTATION  1
#define TOUCH_CS    41           // XPT2046 на общей SPI-шине (T_DO=4), getTouch()

// Touch calibration — rough factory numbers for this panel/orientation.
// If taps land offset from the finger, nudge these (or bump rotation value).
#define TOUCH_CAL_X_MIN  200
#define TOUCH_CAL_X_MAX  3800
#define TOUCH_CAL_Y_MIN  240
#define TOUCH_CAL_Y_MAX  3800
#define SMOOTH_FONT 1
#define SPI_FREQUENCY       16000000   // 16 МГц: пробная частота для LVGL (4 МГц давало видимую протяжку кадров; 27 МГц ранее давало помехи на перемычках). При полосах/шуме — вернуть 4000000.
#define SPI_READ_FREQUENCY  2000000

// ─── App feature flags ─────────────────────────────────────
// Joystick gives genuine, independent UP/DOWN/LEFT/RIGHT (not aliased from
// 2 buttons), so enable row/column grid navigation in MainMenuScreen.
#define DEVICE_HAS_4WAY_NAV
