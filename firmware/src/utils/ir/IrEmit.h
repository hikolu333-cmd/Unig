//
// IrEmit — one shared protocol emitter for the IR attack tools.
//
// The same frame math for every caller (PowerBlast, BrandZap, ...): each
// entry sends the code twice with a short gap (receivers latch on repeat).
// Protocols: NEC / NECext (32-bit), Samsung32, Samsung36, SIRC 12/15/20,
// RC5, RC6, JVC, Sharp, Mitsubishi, Panasonic/Kaseikyo.
//

#pragma once
#include <Arduino.h>
#include <IRsend.h>

namespace IrEmit {

// One blast of a single protocol-encoded code (no repeat, no delay).
inline void fireOnce(IRsend& s, const char* proto, uint32_t address,
                     uint32_t command, uint16_t bits)
{
  if (!strcmp(proto, "NEC")) {
    // Reconstructed 32-bit NEC: address lo/hi (inverted if 8-bit) + cmd + ~cmd
    uint32_t addr = address & 0xFFFF, cmd = command & 0xFF;
    uint32_t a1 = addr & 0xFF;
    uint32_t a2 = (addr > 0xFF) ? ((addr >> 8) & 0xFF) : (~a1 & 0xFF);
    uint32_t frame = (a1 << 24) | (a2 << 16) | (cmd << 8) | (~cmd & 0xFF);
    s.sendNEC(frame, 32);
  } else if (!strcmp(proto, "NECEXT")) {
    // Full 32-bit frame given directly: address<<16 | command
    s.sendNEC(((address & 0xFFFF) << 16) | (command & 0xFFFF), 32);
  } else if (!strcmp(proto, "SAMSUNG")) {
    uint32_t frame = ((address & 0xFF) << 24) | ((~address & 0xFF) << 16)
                   | ((command & 0xFF) << 8) | (~command & 0xFF);
    s.sendSAMSUNG(frame, 32);
  } else if (!strcmp(proto, "SAMSUNG36")) {
    s.sendSamsung36(address & 0xFFFF, command & 0xFFFF);
  } else if (!strcmp(proto, "SONY")) {
    uint32_t frame = (command & 0x7F) | ((address & ((1u << (bits - 7)) - 1)) << 7);
    s.sendSony(frame, bits, 2);
  } else if (!strcmp(proto, "RC5")) {
    uint16_t frame = s.encodeRC5((uint8_t)(address & 0x1F), (uint8_t)(command & 0x3F));
    s.sendRC5(frame, 13);
  } else if (!strcmp(proto, "RC6")) {
    uint64_t frame = s.encodeRC6(address & 0xFFFF, (uint8_t)(command & 0xFF));
    s.sendRC6(frame, 20);
  } else if (!strcmp(proto, "JVC")) {
    uint16_t frame = s.encodeJVC((uint8_t)(address & 0xFF), (uint8_t)(command & 0xFF));
    s.sendJVC(frame, 16, true);
  } else if (!strcmp(proto, "SHARP")) {
    s.sendSharp((uint16_t)(address & 0x1F), (uint16_t)(command & 0xFF));
  } else if (!strcmp(proto, "MITSU")) {
    uint64_t frame = ((uint64_t)(address & 0xFF) << 8) | (command & 0xFF);
    s.sendMitsubishi(frame, 16);
  } else if (!strcmp(proto, "PANA")) {
    uint64_t val = s.encodePanasonic(0x4004, (uint8_t)(address & 0xFF),
                                     (uint8_t)((address >> 8) & 0xFF),
                                     (uint8_t)(command & 0xFF));
    s.sendPanasonic64(val);
  } else if (!strcmp(proto, "KASEIKYO")) {
    // Exact Flipper-style kaseikyo rebuild: 48 bits = vendor(16) + parity(4)
    // + genre1(4) + genre2(4) + cmd(10) + id(2) + xor-parity(8).
    uint32_t a = address;
    uint16_t c = command & 0x3FF;
    uint8_t  id      = (a >> 24) & 3;
    uint16_t vendor  = (a >> 8) & 0xFFFF;
    uint8_t  genre1  = (a >> 4) & 0xF;
    uint8_t  genre2  = a & 0xF;
    uint8_t  d0 = vendor & 0xFF, d1 = vendor >> 8;
    uint8_t  vp = d0 ^ d1;
    vp = (vp & 0xF) ^ (vp >> 4);
    uint8_t  d2 = (vp & 0xF) | (genre1 << 4);
    uint8_t  d3 = genre2 | ((c & 0xF) << 4);
    uint8_t  d4 = (id << 6) | (c >> 4);
    uint8_t  d5 = d2 ^ d3 ^ d4;
    uint64_t frame = ((uint64_t)d0 << 40) | ((uint64_t)d1 << 32)
                   | ((uint64_t)d2 << 24) | ((uint64_t)d3 << 16)
                   | ((uint64_t)d4 << 8) | d5;
    s.sendPanasonic64(frame);
  } else if (!strcmp(proto, "RCA")) {
    // RCA/Thomson: header 4ms/4ms, PDWM 24 bits = addr(4)+cmd(8)+~addr(4)+~cmd(8)
    uint32_t addr = address & 0xF, cmd = command & 0xFF;
    uint32_t frame = addr | (cmd << 4) | ((~addr & 0xF) << 12)
                   | ((~cmd & 0xFF) << 16);
    s.mark(4000); s.space(4000);
    for (int b = 23; b >= 0; b--) {
      s.mark(500);
      s.space(((frame >> b) & 1) ? 2000 : 1000);
    }
  } else if (!strcmp(proto, "NEC42")) {
    // NEC42: address 26 bits + cmd 16 bits = 42 code bits
    s.sendNEC((uint64_t)((address & 0x3FFFFFFULL) << 16) | (command & 0xFFFF), 42);
  } else if (!strcmp(proto, "RC5X")) {
    // RC5X: 7-bit command field split across two nibbles (Philips extension)
    uint8_t  sys  = address & 0x1F;
    uint8_t  cmd  = command & 0x3F;
    uint8_t  hi   = (command >> 6) & 0x3;      // two extra command bits
    uint16_t frame = (uint16_t)((hi << 10) | (sys << 5) | cmd)
                   | 0x3000;                   // start bits + toggle handled by lib
    s.sendRC5(frame, 13);
  } else if (!strcmp(proto, "PIONEER")) {
    // Pioneer: NEC-like 32.5-strong — use RC5-ish? No: Pioneer IR is
    // OOK 40kHz PDWM: header 8500/4250, bit 500/(1250|500). Emit as raw
    // mark/space sequence built from address(8)+~addr(8)+cmd(8)+~cmd(8).
    uint8_t a1 = address & 0xFF, c1 = command & 0xFF;
    uint32_t frame = (a1 << 24) | ((~a1 & 0xFF) << 16)
                   | (c1 << 8) | (~c1 & 0xFF);
    s.mark(8500); s.space(4250);
    for (int b = 31; b >= 0; b--) {
      s.mark(500);
      s.space(((frame >> b) & 1) ? 1250 : 500);
    }
    s.mark(500); s.space(30000);
  }
}

// Standard attack shot: the code twice, 40 ms apart (receiver latch window).
inline void fire(IRsend& s, const char* proto, uint32_t address,
                 uint32_t command, uint16_t bits)
{
  fireOnce(s, proto, address, command, bits);
  delay(40);
  fireOnce(s, proto, address, command, bits);
}

}  // namespace IrEmit
