//
// DIY Joystick (bare ESP32-S3 DevKit + 2.8" ILI9341-family SPI TFT "Serial Port Module A"
// + KY-023 dual-axis analog joystick).
//
// Wiring (VERIFIED on real hardware, 2026-11):
//   Display (SPI):
//     CS=21  DC=47  RST=EN платы (программный сброс, TFT_RST=-1)
//     MOSI=38  SCLK=39  MISO=-1 (SDO дисплея не подключён)
//     LED(BL)=40 (PWM через LEDC)
//     VCC: строго 3V3 (модуль БЕЗ стабилизатора, 5V глушит логику!)
//     Touch (XPT2046): T_CS=41, T_DO=4, T_CLK/T_DIN — перемычки на SCK/MOSI
//     (TOUCH_CS пока -1: тач вводится позже, джойстик — основная навигация)
//
//   KY-023 joystick (VERIFIED: VCC=3V3, VRx=1, VRy=2, SW=42):
//     VRx -> GPIO1  (ADC1_CH0)
//     VRy -> GPIO2  (ADC1_CH1)
//     SW  -> GPIO42 (digital, active LOW, internal pull-up)
//     Both VRx/VRy are on ADC1 (not ADC2), so reading them while WiFi is
//     active is safe.
//
//   Grove-style external I2C (free for NFC/GPS/sensor add-ons later):
//     SDA=17  SCL=18
//
// Flash: 16MB (проверено esptool: Manufacturer 0x68, Device 0x4018).
// SPI-частота дисплея 4 МГц: экранированные перемычки, на 27 МГц уже помехи.
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
#define GROVE_SDA  17
#define GROVE_SCL  18

static const uint8_t SDA = GROVE_SDA;
static const uint8_t SCL = GROVE_SCL;

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

// Panel is an ILI9341 clone that renders photo-negative without INVON.
#define TFT_INVERSION_ON

// KY-023 joystick is unhooked for now: floating VRx/VRy produce random
// directions and wake the screen from power-save (periodic blinking).
// Remove this define once the stick is wired back.
#define JOYSTICK_NOT_WIRED

#define ILI9341_2_DRIVER
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
#define TFT_MOSI    38
#define TFT_SCLK    39
#define TFT_MISO    -1
#define TFT_CS      21
#define TFT_DC      47
#define TFT_RST     -1           // RESET завязан на EN платы — сброс программный
#define TFT_BL      -1           // backlight via LEDC (see LCD_BL)
#define TFT_BACKLIGHT_ON HIGH
#define TFT_DEFAULT_ORIENTATION  1
#define TOUCH_CS    -1           // touch not wired/used yet — joystick is primary nav
#define SMOOTH_FONT 1
#define SPI_FREQUENCY       16000000   // 16 МГц: пробная частота для LVGL (4 МГц давало видимую протяжку кадров; 27 МГц ранее давало помехи на перемычках). При полосах/шуме — вернуть 4000000.
#define SPI_READ_FREQUENCY  2000000

// ─── App feature flags ─────────────────────────────────────
// Joystick gives genuine, independent UP/DOWN/LEFT/RIGHT (not aliased from
// 2 buttons), so enable row/column grid navigation in MainMenuScreen.
#define DEVICE_HAS_4WAY_NAV
