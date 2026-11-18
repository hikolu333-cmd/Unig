//
// UniversalTv — Flipper-style universal TV remote (from flipper tv.ir).
//
// Six buttons; each fires a cross-brand volley of every capture in the
// Flipper universal asset (111 models): parsed protocol codes first, then
// raw captures. Works on most sets without picking a brand at all.
//
// 16MB builds only (raw sample payload); gated off on BOARD_4MB.
//

#pragma once
#include <Arduino.h>
#include <IRsend.h>
#include "IrEmit.h"

#ifndef BOARD_4MB

namespace UniversalTv {

struct UCode { const char* proto; uint32_t address; uint32_t command; uint16_t bits; };
struct URaw  { const uint16_t* samples; uint16_t len; };

#include "UniversalTv.inc"

// Button order matches tv.ir names: Power, Mute, Vol_up, Vol_dn, Ch_next, Ch_prev.
static constexpr int BTN_N      = 6;
static constexpr int BTN_POWER  = 0;
static constexpr int BTN_MUTE   = 1;
static constexpr int BTN_VOLUP  = 2;
static constexpr int BTN_VOLDN  = 3;
static constexpr int BTN_CHUP   = 4;
static constexpr int BTN_CHDN   = 5;

static const char* const BTN_NAMES[BTN_N] =
  { "POWER", "MUTE", "VOL +", "VOL -", "CH +", "CH -" };

struct Btn {
  const UCode* codes;   int codesN;
  const URaw*  raws;    int rawsN;
};

static const Btn BTNS[BTN_N] = {
  { U_POWER,  U_POWER_N,  UR_POWER,  UR_POWER_N  },
  { U_MUTE,   U_MUTE_N,   UR_MUTE,   UR_MUTE_N   },
  { U_VOL_UP, U_VOL_UP_N, UR_VOL_UP, UR_VOL_UP_N },
  { U_VOL_DN, U_VOL_DN_N, UR_VOL_DN, UR_VOL_DN_N },
  { U_CH_NEXT,U_CH_NEXT_N,UR_CH_NEXT,UR_CH_NEXT_N},
  { U_CH_PREV,U_CH_PREV_N,UR_CH_PREV,UR_CH_PREV_N},
};

inline int btnTotal(int btn) {
  return BTNS[btn].codesN + BTNS[btn].rawsN;
}

}  // namespace UniversalTv

#endif  // !BOARD_4MB
