#include "NrfJammer.h"
#include <RF24.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool     NrfJammer::_running = false;
int      NrfJammer::_online  = 0;
uint32_t NrfJammer::_hops    = 0;
void*    NrfJammer::_task    = nullptr;

namespace {
// One full 2.4 GHz ISM band: nRF24 channels 0..125 (2400..2525 MHz).
constexpr uint8_t kBandChannels = 126;
// Per-hop pause. Each module holds its carrier on the current channel this
// long; with 4 modules on a 400 µs dwell the whole band is re-jammed every
// ~12.6 ms and each channel sees a ~1.6 ms carrier hit per pass.
constexpr uint32_t kDwellUs = 400;

RF24*    sRadios[NrfJammer::MAX_MODULES] = {};
uint8_t  sCe[NrfJammer::MAX_MODULES]     = {};
bool     sStarted[NrfJammer::MAX_MODULES] = {};
uint8_t  sNextCh[NrfJammer::MAX_MODULES] = {};
int      sCount = 0;
ExtSpiClass* sSpi = nullptr;
}  // namespace

int NrfJammer::start(ExtSpiClass* spi, const uint8_t* ce, const uint8_t* csn, int count) {
  if (!spi || count <= 0) return 0;
  stop();
  if (count > MAX_MODULES) count = MAX_MODULES;

  sSpi   = spi;
  sCount = count;

  // Bus must be up before the first transaction (deferred-init boards call
  // setPins() only — a re-begin here is harmless for boards that began it).
  if (spi->pinSCK() >= 0)
    spi->begin(spi->pinSCK(), spi->pinMISO(), spi->pinMOSI(), -1);
  delay(10);

  int online = 0;
  for (int m = 0; m < sCount; m++) {
    pinMode(ce[m],  OUTPUT); digitalWrite(ce[m],  LOW);
    pinMode(csn[m], OUTPUT); digitalWrite(csn[m], HIGH);

    auto* r = new RF24(ce[m], csn[m]);
    if (!r->begin(spi)) { delete r; sRadios[m] = nullptr; sStarted[m] = false; continue; }

    // Carrier quality only depends on PA + channel — the packet engine is
    // irrelevant while CONT_WAVE is set. 2 Mbps widens the spectral skirt.
    r->setPALevel(RF24_PA_MAX);
    if (!r->setDataRate(RF24_2MBPS)) r->setDataRate(RF24_1MBPS);
    r->startConstCarrier(RF24_PA_MAX, (uint8_t)m);

    sCe[m]      = ce[m];
    sNextCh[m]  = (uint8_t)m;
    sRadios[m]  = r;
    sStarted[m] = true;
    online++;
  }

  if (online == 0) return 0;

  _online = online;
  _hops   = 0;
  _running = true;
  xTaskCreatePinnedToCore(taskTramp, "nrfjam", 4096, nullptr,
                          3 /* prio: above idle, below UI */, (TaskHandle_t*)&_task, 0);
  return online;
}

void NrfJammer::stop() {
  if (_running) {
    // Task polls _running every ~2 ms and deletes itself — give it a window
    // so the radios aren't torn down under its SPI transactions.
    _running = false;
    vTaskDelay(pdMS_TO_TICKS(50));
    _task = nullptr;
  }
  for (int m = 0; m < MAX_MODULES; m++) {
    if (sRadios[m]) {
      sRadios[m]->stopConstCarrier();
      sRadios[m]->powerDown();
      delete sRadios[m];
      sRadios[m] = nullptr;
    }
    if (sStarted[m]) { digitalWrite(sCe[m], LOW); sStarted[m] = false; }
  }
  _online = 0;
}

void NrfJammer::taskTramp(void*) { sweepLoop(); }

void NrfJammer::sweepLoop() {
  while (_running) {
    for (int m = 0; m < sCount && _running; m++) {
      if (!sRadios[m]) continue;
      sRadios[m]->setChannel(sNextCh[m]);
      sNextCh[m] = (uint8_t)(sNextCh[m] + sCount);
      if (sNextCh[m] >= kBandChannels) sNextCh[m] = (uint8_t)m;  // keep the stride
      _hops++;
      // Short busy-wait: tighter than the 1 ms FreeRTOS tick, keeps the
      // carriers interleaved instead of pulsing in 1 ms bursts.
      delayMicroseconds(kDwellUs);
    }
  }
  vTaskDelete(nullptr);
}
