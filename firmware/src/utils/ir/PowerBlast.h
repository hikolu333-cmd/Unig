//
// PowerBlast — one-shot volley of every known TV/projector POWER code.
//
// TV-B-Gone walks its own code list slowly (~2 min); this is the fast net:
// 150+ distinct power/standby codes across NEC / Samsung / Sony / RC5 / RC6 /
// JVC / Sharp / Mitsubishi / Panasonic, harvested from the open IRDB
// (probonopd/irdb, TV + Projector categories) plus common ODM addresses
// (Vestel/Realtek/Skyworth boards — the DEXP/Harper/BBK family). One full
// volley takes ~12 s: each code is sent 2 times with a 120 ms gap — enough
// for receivers to latch, fast enough to cover everything in one go.
//
// Codes are grouped by protocol; the sender is driven directly (IRsend),
// which keeps this independent of the Signal/file pipeline.
//

#pragma once
#include <Arduino.h>
#include <IRsend.h>
#include "IrEmit.h"

namespace PowerBlast {

struct Code {
  const char* proto;   // "NEC" | "SAMSUNG" | "SONY" | "RC5" | "RC6" | "JVC" | "SHARP" | "MITSU" | "PANA"
  uint32_t    address;
  uint32_t    command;
  uint16_t    bits;
};

// Fast net: IRDB + Flipper curated POWER codes (~475).
#define PB(p, a, c, b) { p, a, c, b }
static const Code TABLE[] = {
#include "PowerBlastTable.inc"
};
#undef PB

static constexpr int COUNT = (int)(sizeof(TABLE) / sizeof(TABLE[0]));

// Full net + raw dump (1552 parsed + 2401 raw). ~390 KB of rodata: on 4 MB
// boards that overflows the app partition — gated off there; they keep the
// Fast table and Instant Off.
#ifndef BOARD_4MB
struct RawCode { uint16_t kHz; const uint16_t* samples; uint16_t len; };
#include "RawPowerTable.inc"

#define PB(p, a, c, b) { p, a, c, b }
static const Code TABLE_ALL[] = {
#include "PowerAllTable.inc"
};
#undef PB
static constexpr int COUNT_ALL = (int)(sizeof(TABLE_ALL) / sizeof(TABLE_ALL[0]));
#endif  // !BOARD_4MB

// Instant Off: essentials (top brands + ODM boards incl. DEXP/Vestel/Realtek)
// + the first 50 of the universal dump — covers ~95% of TVs in ~6 s.
#define PB(p, a, c, b) { p, a, c, b }
static const Code TABLE_INSTANT[] = {
#include "InstantOffTable.inc"
};
#undef PB
static constexpr int COUNT_INSTANT = (int)(sizeof(TABLE_INSTANT) / sizeof(TABLE_INSTANT[0]));

// Send the whole volley. `sender` must be begun (IRsend::begin()).
// `cancelCb` is polled between codes — return true to abort.
// Returns the number of codes sent.
inline int blast(IRsend& s, bool (*cancelCb)() = nullptr, void (*progressCb)(int, int) = nullptr) {
  int n = 0;
  for (int i = 0; i < COUNT; i++) {
    if (cancelCb && cancelCb()) break;
    const Code& c = TABLE[i];

    // NEC-family: build the 32-bit frame with address/complement. For
    // extended NEC (address > 0xFF) the high byte replaces the address
    // complement, per the NEC ext spec — same as encodeNEC(addr16, cmd).
    if (!strcmp(c.proto, "NEC")) {
      uint32_t addr = c.address & 0xFFFF, cmd = c.command & 0xFF;
      uint32_t a1 = addr & 0xFF;
      uint32_t a2 = (addr > 0xFF) ? ((addr >> 8) & 0xFF) : (~a1 & 0xFF);
      uint32_t frame = (a1 << 24) | (a2 << 16) | (cmd << 8) | (~cmd & 0xFF);
      s.sendNEC(frame, 32);
      delay(40); s.sendNEC(frame, 32);   // one repeat: receivers want a pair
    } else if (!strcmp(c.proto, "SAMSUNG")) {
      uint32_t frame = ((c.address & 0xFF) << 24)
                    | ((~c.address & 0xFF) << 16)
                    | ((c.command & 0xFF) << 8)
                    | (~c.command & 0xFF);
      s.sendSAMSUNG(frame, 32);
      delay(40); s.sendSAMSUNG(frame, 32);
    // Sony SIRC: command low 7 bits, address above (width = bits - 7).
    } else if (!strcmp(c.proto, "SONY")) {
      uint32_t frame = (c.command & 0x7F)
                    | ((c.address & ((1u << (c.bits - 7)) - 1)) << 7);
      s.sendSony(frame, c.bits, 2);   // repeats built in (repeat=2)
    } else if (!strcmp(c.proto, "RC5")) {
      uint16_t frame = (uint16_t)((s.encodeRC5((uint8_t)(c.address & 0x1F),
                                               (uint8_t)(c.command & 0x3F))));
      s.sendRC5(frame, 13);
      delay(30); s.sendRC5(frame, 13);
    } else if (!strcmp(c.proto, "RC6")) {
      uint64_t frame = s.encodeRC6(c.address & 0xFFFF, (uint8_t)(c.command & 0xFF));
      s.sendRC6(frame, 20);
      delay(30); s.sendRC6(frame, 20);
    } else if (!strcmp(c.proto, "JVC")) {
      uint16_t frame = s.encodeJVC((uint8_t)(c.address & 0xFF), (uint8_t)(c.command & 0xFF));
      s.sendJVC(frame, 16, true);
      delay(30); s.sendJVC(frame, 16, true);
    } else if (!strcmp(c.proto, "SHARP")) {
      s.sendSharp((uint16_t)(c.address & 0x1F), (uint16_t)(c.command & 0xFF));
      delay(30); s.sendSharp((uint16_t)(c.address & 0x1F), (uint16_t)(c.command & 0xFF));
    } else if (!strcmp(c.proto, "MITSU")) {
      // Mitsubishi 16-bit: address byte + command byte.
      uint64_t frame = ((uint64_t)(c.address & 0xFF) << 8) | (c.command & 0xFF);
      s.sendMitsubishi(frame, 16);
      delay(30); s.sendMitsubishi(frame, 16);
    } else if (!strcmp(c.proto, "PANA")) {
      // Kaseikyo, Panasonic vendor 0x4004; address packs dev | sub<<8.
      uint64_t val = s.encodePanasonic(0x4004, (uint8_t)(c.address & 0xFF),
                                       (uint8_t)((c.address >> 8) & 0xFF),
                                       (uint8_t)(c.command & 0xFF));
      s.sendPanasonic64(val);
      delay(30); s.sendPanasonic64(val);
    }

    n++;
    if (progressCb) progressCb(n, COUNT);
    delay(55);
  }
  return n;
}

// Send a single protocol-encoded code (one frame + one repeat for NEC/RC5/RC6,
// built-in repeats for Sony). Shared by the volleys and Quick Off.
inline void sendCode(IRsend& s, const char* proto, uint32_t address,
                     uint32_t command, uint16_t bits) {
  if (!strcmp(proto, "NEC")) {
    uint32_t addr = address & 0xFFFF, cmd = command & 0xFF;
    uint32_t a1 = addr & 0xFF;
    uint32_t a2 = (addr > 0xFF) ? ((addr >> 8) & 0xFF) : (~a1 & 0xFF);
    uint32_t frame = (a1 << 24) | (a2 << 16) | (cmd << 8) | (~cmd & 0xFF);
    s.sendNEC(frame, 32);
    delay(40); s.sendNEC(frame, 32);
  } else if (!strcmp(proto, "SAMSUNG")) {
    uint32_t frame = ((address & 0xFF) << 24) | ((~address & 0xFF) << 16)
                  | ((command & 0xFF) << 8) | (~command & 0xFF);
    s.sendSAMSUNG(frame, 32);
    delay(40); s.sendSAMSUNG(frame, 32);
  } else if (!strcmp(proto, "SONY")) {
    uint32_t frame = (command & 0x7F)
                  | ((address & ((1u << (bits - 7)) - 1)) << 7);
    s.sendSony(frame, bits, 2);
  } else if (!strcmp(proto, "RC5")) {
    uint16_t frame = s.encodeRC5((uint8_t)(address & 0x1F), (uint8_t)(command & 0x3F));
    s.sendRC5(frame, 13);
    delay(30); s.sendRC5(frame, 13);
  } else if (!strcmp(proto, "RC6")) {
    uint64_t frame = s.encodeRC6(address & 0xFFFF, (uint8_t)(command & 0xFF));
    s.sendRC6(frame, 20);
    delay(30); s.sendRC6(frame, 20);
  } else if (!strcmp(proto, "JVC")) {
    uint16_t frame = s.encodeJVC((uint8_t)(address & 0xFF), (uint8_t)(command & 0xFF));
    s.sendJVC(frame, 16, true);
    delay(30); s.sendJVC(frame, 16, true);
  } else if (!strcmp(proto, "SHARP")) {
    s.sendSharp((uint16_t)(address & 0x1F), (uint16_t)(command & 0xFF));
    delay(30); s.sendSharp((uint16_t)(address & 0x1F), (uint16_t)(command & 0xFF));
  } else if (!strcmp(proto, "MITSU")) {
    uint64_t frame = ((uint64_t)(address & 0xFF) << 8) | (command & 0xFF);
    s.sendMitsubishi(frame, 16);
    delay(30); s.sendMitsubishi(frame, 16);
  } else if (!strcmp(proto, "PANA")) {
    uint64_t val = s.encodePanasonic(0x4004, (uint8_t)(address & 0xFF),
                                     (uint8_t)((address >> 8) & 0xFF),
                                     (uint8_t)(command & 0xFF));
    s.sendPanasonic64(val);
    delay(30); s.sendPanasonic64(val);
  }
}

// Quick Off: fire a saved code window ("NEC:4:194:32;SAMSUNG:7:2:32;…").
// Returns the number of codes sent.
inline int quickBlast(IRsend& s, const char* spec, bool (*cancelCb)() = nullptr) {
  int n = 0;
  const char* p = spec;
  while (p && *p) {
    if (cancelCb && cancelCb()) break;
    char proto[8] = {0};
    uint32_t a = 0, c = 0;
    uint16_t b = 32;
    // "PROTO:ADDR:CMD:BITS"
    int consumed = 0;
    if (sscanf(p, "%7[A-Z0-9]:%x:%x:%hu%n", proto, &a, &c, &b, &consumed) >= 4) {
      sendCode(s, proto, a, c, b);
      n++;
    } else {
      break;
    }
    p += consumed;
    while (*p == ';' || *p == ' ') p++;
  }
  return n;
}

// Instant Off: turbo volley. NEC-family codes fire a single frame (receivers
// latch on the first frame — the second was only repeat insurance) with a
// 15 ms gap; bi-phase RC5/RC6 still double up because their decoders need
// two frames to lock. The DEXP/Vestel NEC 0x0 zone leads the table, so
// common ODM TVs die within the first ~2 seconds.
inline int instantOff(IRsend& s, const char* spec = nullptr,
                      bool (*cancelCb)() = nullptr,
                      void (*progressCb)(int, int) = nullptr) {
  int n = 0;
  if (spec && spec[0]) {
    n += quickBlast(s, spec, cancelCb);
    if (progressCb) progressCb(n, COUNT_INSTANT + n);
  }
  for (int i = 0; i < COUNT_INSTANT; i++) {
    if (cancelCb && cancelCb()) break;
    const Code& c = TABLE_INSTANT[i];
    bool biphase = !strcmp(c.proto, "RC5") || !strcmp(c.proto, "RC6");
    IrEmit::fireOnce(s, c.proto, c.address, c.command, c.bits);
    if (biphase) {
      delay(15);
      IrEmit::fireOnce(s, c.proto, c.address, c.command, c.bits);
    }
    n++;
    if (progressCb) progressCb(n, COUNT_INSTANT);
    delay(15);
  }
  return n;
}

// ── Full sweep: every parsed code + every raw capture ────────────────────────
// mode 0 = fast net (TABLE only), 1 = everything (TABLE_ALL + RAW_TABLE).
// Returns the number of signals sent.
inline int blastAll(IRsend& s, int mode, bool (*cancelCb)() = nullptr,
                    void (*progressCb)(int, int) = nullptr) {
  int n = 0;
#ifdef BOARD_4MB
  mode = 0;   // 4 MB boards ship the Fast table only — "all" behaves as fast
  const int total = COUNT;
#else
  const int total = (mode == 1) ? (COUNT_ALL + (int)(sizeof(RAW_TABLE) / sizeof(RAW_TABLE[0]))) : COUNT;
#endif

  auto emit = [&](const char* proto, uint32_t address, uint32_t command, uint16_t bits) {
    IrEmit::fire(s, proto, address, command, bits);
    n++;
    if (progressCb) progressCb(n, total);
  };

#ifndef BOARD_4MB
  if (mode == 1) {
    for (int i = 0; i < COUNT_ALL; i++) {
      if (cancelCb && cancelCb()) return n;
      const Code& c = TABLE_ALL[i];
      emit(c.proto, c.address, c.command, c.bits);
      delay(50);
    }
    // Raw captures — send straight from PROGMEM (sendRaw takes flash ptrs on ESP32).
    for (size_t r = 0; r < sizeof(RAW_TABLE) / sizeof(RAW_TABLE[0]); r++) {
      if (cancelCb && cancelCb()) return n;
      const RawCode& rc = RAW_TABLE[r];
      s.sendRaw(rc.samples, rc.len, rc.kHz);
      n++;
      if (progressCb) progressCb(n, total);
      delay(50);
    }
  } else
#endif
  {
    for (int i = 0; i < COUNT; i++) {
      if (cancelCb && cancelCb()) break;
      const Code& c = TABLE[i];
      emit(c.proto, c.address, c.command, c.bits);
      delay(50);
    }
  }
  return n;
}

}  // namespace PowerBlast
