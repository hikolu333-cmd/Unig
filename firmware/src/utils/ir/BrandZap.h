//
// BrandZap — brand-targeted IR volley (v2: per-command ranges).
//
// Pick a device class (TV / projector), pick a brand — the zap fires every
// unique code of THAT brand for the chosen command (Power / Mute / Vol+ /
// Vol-), harvested from the brand's remotes across models. Neighboring TVs
// stay untouched. "All Projectors" sweeps every projector brand's POWER set.
//
// DEXP is synthesized from the verified Vestel ODM NEC 0x0 family (the zone
// that killed the real test set) plus the stock Vestel vol/mute commands.
//
// The volley is non-blocking: one code per frame (IrEmit), BACK cancels.
//

#pragma once
#include <Arduino.h>
#include <IRsend.h>
#include "IrEmit.h"

namespace BrandZap {

static constexpr int CMD_POWER = 0;
static constexpr int CMD_MUTE  = 1;
static constexpr int CMD_VOLUP = 2;
static constexpr int CMD_VOLDN = 3;
static constexpr int CMD_N     = 4;

struct Code {
  const char* proto;   // IrEmit protocol id
  uint32_t    address;
  uint32_t    command;
  uint16_t    bits;
  uint8_t     cmdId;   // CMD_*
};

struct Range { uint16_t first; uint16_t count; };

struct Entry {
  const char* brand;
  uint8_t     cat;      // 0 = TV, 1 = projector
  Range       ranges[CMD_N];
};

#include "BrandZap.inc"   // ENTRIES, ENTRY_COUNT, CODES, CODE_COUNT
#include "XiaomiRaw.inc"  // RAW_SIGS, RAW_SIG_COUNT (raw-only brand)

// Emit a raw capture straight from samples (Xiaomi Mi TV — no parsed form).
inline void fireRaw(IRsend& s, const RawSig& r) {
  s.sendRaw(r.samples, r.len, r.kHz);
}

static constexpr int CAT_TV   = 0;
static constexpr int CAT_PROJ = 1;

inline int brandCount(int cat) {
  int n = 0;
  for (int i = 0; i < ENTRY_COUNT; i++)
    if (ENTRIES[i].cat == cat) n++;
  return n;
}

// n-th brand of a category.
inline const Entry* brandAt(int cat, int n) {
  int k = 0;
  for (int i = 0; i < ENTRY_COUNT; i++) {
    if (ENTRIES[i].cat != cat) continue;
    if (k == n) return &ENTRIES[i];
    k++;
  }
  return nullptr;
}

inline int zapTotal(const Entry* e, int cmdId) {
  return (e && cmdId >= 0 && cmdId < CMD_N) ? e->ranges[cmdId].count : 0;
}

// All-projectors sweep total (POWER codes only).
inline int zapAllProjectorsTotal() {
  int n = 0;
  for (int i = 0; i < ENTRY_COUNT; i++)
    if (ENTRIES[i].cat == CAT_PROJ) n += ENTRIES[i].ranges[CMD_POWER].count;
  return n;
}

}  // namespace BrandZap
