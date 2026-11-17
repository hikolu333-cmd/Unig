//
// NrfJammer — multi-module 2.4 GHz sweep jammer.
//
// Parks every nRF24L01+ on a constant carrier (CONT_WAVE | PLL_LOCK, max PA)
// and sweeps the modules in lockstep: module m takes channels m, m+n, m+2n…
// so n modules hold n carriers at any instant and the whole 2.4 GHz ISM band
// (126 × 1 MHz channels: WiFi, BT/BLE, Zigbee, RC, analog video) is revisited
// every few tens of ms.
//
// Runs in its own FreeRTOS task (core 0 — Arduino loop lives on core 1) so
// the sweep rate doesn't depend on the UI loop. UI only calls start()/stop()
// and reads hops() for the status line.
//

#pragma once
#include <Arduino.h>
#include "core/ExtSpiClass.h"

class NrfJammer {
public:
  static constexpr int MAX_MODULES = 4;

  // Bring every module up and start the sweep task. ce/csn are arrays of
  // `count` GPIO numbers on the shared SPI bus. Returns the number of
  // modules that actually responded (0 = nothing started).
  static int  start(ExtSpiClass* spi, const uint8_t* ce, const uint8_t* csn, int count);
  static void stop();               // stop sweep + power every module down
  static bool active()              { return _running; }
  static int  online()              { return _online; }
  static uint32_t hops()            { return _hops; }

private:
  static void taskTramp(void* arg);
  static void sweepLoop();

  static bool       _running;
  static int        _online;
  static uint32_t   _hops;
  static void*      _task;
};
