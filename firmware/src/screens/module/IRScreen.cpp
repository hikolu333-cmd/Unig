//
// IR Remote Screen
//

#include "IRScreen.h"
#ifdef USE_LVGL_SHELL
#include "screens/module/IRBrandZapScreen.h"
#endif
#include "ui/actions/ShowStatusAction.h"
#include "utils/ir/PowerBlast.h"
#include "utils/uart/UartFileManager.h"

#if defined(DEVICE_M5STICK_S3)
#include <M5PM1.h>
#include <Wire.h>
extern M5PM1 pm1;
static void _irAmpEnable(bool on) {
  // Class-D amp enable via M5PM1 GPIO3
  pm1.gpioSet(M5PM1_GPIO_NUM_3, M5PM1_GPIO_MODE_OUTPUT, on ? 1 : 0,
              M5PM1_GPIO_PULL_NONE, M5PM1_GPIO_DRIVE_PUSHPULL);
}
#endif

#include "core/AchievementManager.h"
#include "core/Device.h"
#include "core/IStorage.h"
#include "core/ScreenManager.h"
#include "core/PinConfigManager.h"
#include "screens/module/ModuleMenuScreen.h"
#include "ui/actions/ShowStatusAction.h"
#include "ui/actions/InputTextAction.h"
#include "ui/views/ProgressView.h"
#include "ui/actions/InputNumberAction.h"
#include "ui/actions/InputSelectAction.h"

IRScreen* IRScreen::_activeInstance = nullptr;

void IRScreen::onInit() {
  _txPin = (int8_t)PinConfig.getInt(PIN_CONFIG_IR_TX, PIN_CONFIG_IR_TX_DEFAULT);
  _rxPin = (int8_t)PinConfig.getInt(PIN_CONFIG_IR_RX, PIN_CONFIG_IR_RX_DEFAULT);

  render();
  ProgressView::init();
  ProgressView::progress("Starting IR...", 30);

  if (_pendingSendFile.length() > 0) {
    _openPendingSendFile();
    return;
  }
  ProgressView::finish();
  _showMenu();
}

// Launched from the File Manager: skip the menu and open the Send signal-list
// for a specific .ir file, ready to fire each command.
void IRScreen::_openPendingSendFile() {
  String file = _pendingSendFile;
  _pendingSendFile = "";

  if (_txPin < 0) {
    ShowStatusAction::show("TX pin not set");
    Screen.goBack();
    return;
  }
  #if defined(DEVICE_M5STICK_S3)
  if (_txPin == IR_TX_PIN) Uni.Power.setExtOutput(true);
  _irAmpEnable(true);
  #endif
  _ir.beginTx(_txPin);

  // Parent folder so BACK from the send-list returns to a browse of that dir.
  int slash = file.lastIndexOf('/');
  _browsePath = (slash > 0) ? file.substring(0, slash) : String(kRootPath);

  _loadAndShowSignals(file);
  if (_state != STATE_SEND_LIST) {   // read/parse failed — bounce back
    _ir.end();
    Screen.goBack();
  }
}

void IRScreen::_showMenu() {
  _state = STATE_MENU;
  _ir.end();
  #if defined(DEVICE_M5STICK_S3)
  Uni.Power.setExtOutput(false);
  _irAmpEnable(true);
  #endif
  strncpy(_titleBuf, "IR Remote", sizeof(_titleBuf));
  // Re-read pins in case they were changed under Settings > Pin Setting.
  _txPin = (int8_t)PinConfig.getInt(PIN_CONFIG_IR_TX, PIN_CONFIG_IR_TX_DEFAULT);
  _rxPin = (int8_t)PinConfig.getInt(PIN_CONFIG_IR_RX, PIN_CONFIG_IR_RX_DEFAULT);
#ifdef USE_LVGL_SHELL
  setItems(_menuItems, 8, _menuReturnIndex);
#else
  setItems(_menuItems, 7, _menuReturnIndex);
#endif
}

void IRScreen::onUpdate() {
  if (_state == STATE_RECEIVING) {
    IRUtil::Signal sig;
    if (_ir.receive(sig)) {
      if (_capturedCount < IRUtil::MAX_SIGNALS && !_isDuplicate(sig)) {
        sig.name = "Signal " + String(_capturedCount + 1);
        _captured[_capturedCount] = sig;
        _capturedCount++;
        #if defined(DEVICE_M5STICK_S3) && defined(IR_RX_PIN)
        if (Uni.Speaker && _rxPin != IR_RX_PIN) Uni.Speaker->playNotification();
        #else
        if (Uni.Speaker) Uni.Speaker->playNotification();
        #endif
        if (_capturedCount == 1) {
          int n = Achievement.inc("ir_receive_first");
          if (n == 1) Achievement.unlock("ir_receive_first");
        }
        _showReceiveList();
      }
      _ir.resumeReceive();
    }

    // Blink indicator
    if (millis() - _lastRender > 500) {
      _lastRender = millis();
      render();
    }
  }

  // Long-press to open action menu on send list signals
  if (_state == STATE_SEND_LIST && !_holdFired &&
      Uni.Nav->isPressed() && Uni.Nav->heldDuration() >= 1000) {
    _holdFired = true;
    if (_selectedIndex < _sendCount) {
      _onSendItemAction(_selectedIndex);
    }
    return;
  }

  if (_holdFired) {
    if (Uni.Nav->wasPressed()) {
      Uni.Nav->readDirection();
      _holdFired = false;
    }
    return;
  }

  ListScreen::onUpdate();
}

void IRScreen::onRender() {
  ListScreen::onRender();
}

void IRScreen::onBack() {
  if (_state == STATE_MENU) {
    _ir.end();
    #if defined(DEVICE_M5STICK_S3)
    Uni.Power.setExtOutput(GROVE_5V_OUTPUT);
    #endif
    Screen.goBack();
  } else if (_state == STATE_RECEIVING) {
    _ir.end();
    _showMenu();
  } else if (_state == STATE_SEND_BROWSE) {
    // Climb to parent if below kRootPath; at the screen's root, exit to menu.
    // Keeps the picker confined to /unigeek/ir — no whole-SD-card browse.
    if (_browsePath == kRootPath || _browsePath.length() == 0) {
      _showMenu();
    } else {
      int slash = _browsePath.lastIndexOf('/');
      String parent = (slash > 0) ? _browsePath.substring(0, slash) : kRootPath;
      _loadBrowseDir(parent);
    }
  } else if (_state == STATE_SEND_LIST) {
    _loadBrowseDir(_browsePath);
  } else {
    _showMenu();
  }
}

void IRScreen::onItemSelected(uint8_t index) {
  if (_state == STATE_MENU) {
    _menuReturnIndex = index;
    switch (index) {
      case 0: { // Quick Off — instant, no diving: saved window + top codes
        _quickOff();
        break;
      }

      case 1: { // Receive
        if (_rxPin < 0) {
          ShowStatusAction::show("RX pin not set");
          render();
          return;
        }
        #if defined(DEVICE_M5STICK_S3)
        if (_rxPin == IR_RX_PIN || _txPin == IR_TX_PIN) {
          Uni.Power.setExtOutput(true);
          if (_rxPin == IR_RX_PIN) {
            _irAmpEnable(false);
            ShowStatusAction::show("Starting...", 2000);
          }
        }
        #endif
        _ir.beginRx(_rxPin);
        if (_txPin >= 0) _ir.beginTx(_txPin);
        _capturedCount = 0;
        _state = STATE_RECEIVING;
        setItems(_recvItems, 0);  // prime pointer once; _showReceiveList uses setCount after this
        break;
      }
      case 2: { // Send
        if (_txPin < 0) {
          ShowStatusAction::show("TX pin not set");
          render();
          return;
        }
        #if defined(DEVICE_M5STICK_S3)
        if (_txPin == IR_TX_PIN) Uni.Power.setExtOutput(true);
        #endif
        _ir.beginTx(_txPin);
        Uni.Storage->makeDir(kRootPath);
        _loadBrowseDir(kRootPath);
        break;
      }
      case 3: { // Find Code — sweep in bursts until the user says "it died"
        _findCode();
        break;
      }

      case 4: { // TV-B-Gone
        if (_txPin < 0) {
          ShowStatusAction::show("TX pin not set");
          render();
          return;
        }

        static constexpr InputSelectAction::Option regionOpts[] = {
          {"North America", "na"},
          {"Europe", "eu"},
        };
        const char* sel = InputSelectAction::popup("Region", regionOpts, 2);
        if (!sel) { render(); return; }
        render();

        #if defined(DEVICE_M5STICK_S3)
        if (_txPin == IR_TX_PIN) Uni.Power.setExtOutput(true);
        #endif
        _ir.beginTx(_txPin);
        _tvbCancelled = false;
        _activeInstance = this;
        _state = STATE_TVBGONE;
        {
          int n = Achievement.inc("ir_tvbgone");
          if (n == 1) Achievement.unlock("ir_tvbgone");
        }

        uint8_t region = (strcmp(sel, "na") == 0) ? 1 : 0;
        ProgressView::init();
        _ir.startTvBGone(region, _tvbProgressCb, _tvbCancelCb);

        _ir.end();
        ProgressView::finish();
        _activeInstance = nullptr;

        if (_tvbCancelled) {
          ShowStatusAction::show("Stopped", 1000);
        } else {
          ShowStatusAction::show("All codes sent", 1500);
          int n = Achievement.inc("ir_tvbgone_complete");
          if (n == 1) Achievement.unlock("ir_tvbgone_complete");
        }
        _showMenu();
        break;
      }

      case 5: { // Power Blast — fast volley, or the full universal dump
        if (_txPin < 0) {
          ShowStatusAction::show("TX pin not set");
          render();
          return;
        }

#ifdef BOARD_4MB
        const char* mode = "fast";
#else
        static constexpr InputSelectAction::Option modeOpts[] = {
          {"Fast (475 codes)", "fast"},
          {"All Devices (3952)", "all"},
        };
        const char* mode = InputSelectAction::popup("Blast Mode", modeOpts, 2);
        if (!mode) { render(); return; }
        render();
#endif

        _ir.beginTx(_txPin);
        _tvbCancelled = false;
        _activeInstance = this;
        _state = STATE_POWERBLAST;

        ProgressView::init();
        const int sent = PowerBlast::blastAll(
            *_ir.sender(), (strcmp(mode, "all") == 0) ? 1 : 0,
            _pbCancelCb, _pbProgressCb);
        _ir.end();
        ProgressView::finish();
        _activeInstance = nullptr;

        if (_tvbCancelled && sent < PowerBlast::COUNT) {
          ShowStatusAction::show("Stopped", 1000);
        } else {
          char msg[40];
          snprintf(msg, sizeof(msg), "Blasted %d power codes", sent);
          ShowStatusAction::show(msg, 1500);
          int n = Achievement.inc("ir_power_blast");
          if (n == 1) Achievement.unlock("ir_power_blast");
        }
        _showMenu();
        break;
      }

      case 6: { // Instant Off — top-62 net, ~6 s, no dialogs
        if (_txPin < 0) {
          ShowStatusAction::show("TX pin not set");
          render();
          return;
        }
        _ir.beginTx(_txPin);
        _tvbCancelled = false;
        _activeInstance = this;
        _state = STATE_POWERBLAST;
        ProgressView::init();
        const int sent = PowerBlast::instantOff(*_ir.sender(), nullptr, _pbCancelCb, _pbProgressCb);
        _ir.end();
        ProgressView::finish();
        _activeInstance = nullptr;
        {
          char msg[40];
          snprintf(msg, sizeof(msg), "%d codes, ~%ds", sent, sent / 10);
          ShowStatusAction::show(msg, 1500);
        }
        _showMenu();
        break;
      }

#ifdef USE_LVGL_SHELL
      case 7: { // Brand Zap — brand-scoped POWER volley (TV/projector library)
        Screen.push(new IRBrandZapScreen());
        break;
      }
#endif
    }
    return;
  }

  if (_state == STATE_RECEIVING) {
    // Last item is "Save Remote"
    if (index == _capturedCount) {
      if (_capturedCount == 0) {
        ShowStatusAction::show("No signals captured");
        render();
        return;
      }
      String filename = InputTextAction::popup("File Name", "remote");
      if (filename.length() == 0) { render(); return; }

      String content = IRUtil::saveToString(_captured, _capturedCount);
      String path = String(kRootPath) + "/" + filename + ".ir";
      Uni.Storage->makeDir(kRootPath);
      if (Uni.Storage->writeFile(path.c_str(), content.c_str())) {
        ShowStatusAction::show("Saved");
        {
          int n = Achievement.inc("ir_signal_saved");
          if (n == 1)  Achievement.unlock("ir_signal_saved");
          if (n == 5)  Achievement.unlock("ir_signal_saved_5");
          if (n == 20) Achievement.unlock("ir_signal_saved_20");
        }
        if (_capturedCount >= 20) {
          int n = Achievement.inc("ir_remote_collection");
          if (n == 1) Achievement.unlock("ir_remote_collection");
        }
      } else {
        ShowStatusAction::show("Failed");
      }
      _ir.end();
      _showMenu();
      return;
    }

    _onRecvItemAction(index);
    return;
  }

  if (_state == STATE_SEND_BROWSE) {
    if (index < _browser.count()) {
      if (_browser.entry(index).isDir) {
        _loadBrowseDir(_browser.entry(index).path);
      } else {
        _loadAndShowSignals(_browser.entry(index).path);
      }
    }
    return;
  }

  if (_state == STATE_SEND_LIST) {
    if (_sendDirty && index == _sendCount) {
      _saveSendFile();
      return;
    }
    if (index < _sendCount) {
      ShowStatusAction::show("Sending...", 0);
      _ir.sendSignal(_sendSignals[index]);
      delay(100);
      {
        int n = Achievement.inc("ir_send_first");
        if (n == 1) Achievement.unlock("ir_send_first");
      }
      ShowStatusAction::show("Sent", 800);
      render();
    }
    return;
  }
}

// ── Receive helpers ─────────────────────────────────────────────────────────

void IRScreen::_showReceiveList() {
  for (uint8_t i = 0; i < _capturedCount; i++) {
    _recvLabels[i] = _captured[i].name;
    _recvSublabels[i] = _signalSublabel(_captured[i]);
    _recvItems[i] = {_recvLabels[i].c_str(), _recvSublabels[i].c_str()};
  }
  _recvItems[_capturedCount] = {">> Save Remote", nullptr};
  setCount(_capturedCount + 1);  // update count, clamp selection — no render
  render();                      // one render at the current selection
}

bool IRScreen::_isDuplicate(const IRUtil::Signal& sig) {
  String fp = _signalFingerprint(sig);
  for (uint8_t i = 0; i < _capturedCount; i++) {
    if (_signalFingerprint(_captured[i]) == fp) return true;
  }
  return false;
}

String IRScreen::_signalFingerprint(const IRUtil::Signal& sig) {
  if (!sig.isRaw && sig.protocol.length() > 0) {
    return sig.protocol + ":" + String(sig.address, HEX) + ":" + String(sig.command, HEX);
  }
  return sig.rawData;
}

String IRScreen::_signalSublabel(const IRUtil::Signal& sig) {
  if (!sig.isRaw && sig.protocol.length() > 0) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %04lX:%04lX", sig.protocol.c_str(),
             (unsigned long)sig.address, (unsigned long)sig.command);
    return String(buf);
  }
  // For raw, show frequency and data length
  int count = 1;
  for (int i = 0; i < (int)sig.rawData.length(); i++) {
    if (sig.rawData[i] == ' ') count++;
  }
  char buf[24];
  snprintf(buf, sizeof(buf), "RAW %uHz %dpts", sig.frequency, count);
  return String(buf);
}

void IRScreen::_onRecvItemAction(uint8_t index) {
  if (index >= _capturedCount) return;

  static constexpr InputSelectAction::Option actionOpts[] = {
    {"Replay", "replay"},
    {"Rename", "rename"},
    {"Delete", "delete"},
  };
  const char* sel = InputSelectAction::popup(_captured[index].name.c_str(), actionOpts, 3);

  if (!sel) {
    render();
    return;
  }

  if (strcmp(sel, "replay") == 0) {
    if (_txPin < 0) {
      ShowStatusAction::show("TX pin not set");
    } else {
      #if defined(DEVICE_M5STICK_S3)
      if (_txPin == IR_TX_PIN) Uni.Power.setExtOutput(true);
      #endif
      _ir.beginTx(_txPin);
      ShowStatusAction::show("Sending...", 0);
      _ir.sendSignal(_captured[index]);
      delay(100);
      {
        int n = Achievement.inc("ir_send_first");
        if (n == 1) Achievement.unlock("ir_send_first");
      }
      ShowStatusAction::show("Sent", 800);
    }
  } else if (strcmp(sel, "rename") == 0) {
    String newName = InputTextAction::popup("New Name", _captured[index].name);
    if (newName.length() > 0) {
      _captured[index].name = newName;
    }
  } else if (strcmp(sel, "delete") == 0) {
    for (uint8_t i = index; i < _capturedCount - 1; i++) {
      _captured[i] = _captured[i + 1];
    }
    _capturedCount--;
  }

  _showReceiveList();
}

// ── Send — file browser ────────────────────────────────────────────────────

void IRScreen::_loadBrowseDir(const String& path) {
  _state = STATE_SEND_BROWSE;
  _browsePath = path;

  int lastSlash = path.lastIndexOf('/');
  String folderName = (lastSlash >= 0) ? path.substring(lastSlash + 1) : path;
  if (path == kRootPath) folderName = "IR Files";
  snprintf(_titleBuf, sizeof(_titleBuf), "%s", folderName.c_str());

  // Root at "/" so a ".." row is shown even at kRootPath, letting the picker
  // climb to the SD card and reach .ir files outside /unigeek/ir.
  _browser.root = "/";
  uint8_t n = _browser.load(this, path, ".ir");

  if (n == 0 && path == kRootPath) {
    ShowStatusAction::show("No IR files");
    _showMenu();
    return;
  }

  setItems(_browser.items(), n);
}

void IRScreen::_loadAndShowSignals(const String& filePath) {
  String content = Uni.Storage->readFile(filePath.c_str());
  if (content.length() == 0) {
    ShowStatusAction::show("Failed to read file");
    render();
    return;
  }

  _sendCount = IRUtil::loadFile(content, _sendSignals, IRUtil::MAX_SIGNALS);
  if (_sendCount == 0) {
    ShowStatusAction::show("No signals in file");
    render();
    return;
  }

  _sendFilePath = filePath;
  _sendDirty = false;

  // Update title to filename
  int lastSlash = filePath.lastIndexOf('/');
  String fileName = (lastSlash >= 0) ? filePath.substring(lastSlash + 1) : filePath;
  snprintf(_titleBuf, sizeof(_titleBuf), "%s", fileName.c_str());

  _state = STATE_SEND_LIST;
  _refreshSendList();
}

void IRScreen::_refreshSendList() {
  for (uint8_t i = 0; i < _sendCount; i++) {
    _sendLabels[i] = _sendSignals[i].name;
    _sendSublabels[i] = _signalSublabel(_sendSignals[i]);
    _sendItems[i] = {_sendLabels[i].c_str(), _sendSublabels[i].c_str()};
  }
  uint8_t total = _sendCount;
  if (_sendDirty) {
    _sendItems[_sendCount] = {">> Save Update", nullptr};
    total++;
  }
  setItems(_sendItems, total);
}

void IRScreen::_onSendItemAction(uint8_t index) {
  if (index >= _sendCount) return;

  static constexpr InputSelectAction::Option opts[] = {
    {"Replay", "replay"},
    {"Rename", "rename"},
    {"Delete", "delete"},
  };
  const char* sel = InputSelectAction::popup(_sendSignals[index].name.c_str(), opts, 3);
  if (!sel) { render(); return; }

  if (strcmp(sel, "replay") == 0) {
    ShowStatusAction::show("Sending...", 0);
    _ir.sendSignal(_sendSignals[index]);
    delay(100);
    {
      int n = Achievement.inc("ir_send_first");
      if (n == 1) Achievement.unlock("ir_send_first");
    }
    ShowStatusAction::show("Sent", 800);
  } else if (strcmp(sel, "rename") == 0) {
    String newName = InputTextAction::popup("New Name", _sendSignals[index].name);
    if (newName.length() > 0) {
      _sendSignals[index].name = newName;
      _sendDirty = true;
    }
  } else if (strcmp(sel, "delete") == 0) {
    for (uint8_t i = index; i < _sendCount - 1; i++) {
      _sendSignals[i] = _sendSignals[i + 1];
    }
    _sendCount--;
    _sendDirty = true;
  }

  _refreshSendList();
}

void IRScreen::_saveSendFile() {
  String content = IRUtil::saveToString(_sendSignals, _sendCount);
  if (Uni.Storage->writeFile(_sendFilePath.c_str(), content.c_str())) {
    ShowStatusAction::show("Saved");
    _sendDirty = false;
  } else {
    ShowStatusAction::show("Failed");
  }
  _refreshSendList();
}

// ── TV-B-Gone callbacks ─────────────────────────────────────────────────────

void IRScreen::_tvbProgressCb(uint8_t current, uint8_t total) {
  if (!_activeInstance) return;
  int pct = (int)((uint32_t)current * 100 / total);
  char msg[32];
  snprintf(msg, sizeof(msg), "Sending %u / %u", current, total);
  ProgressView::progress(msg, pct);
}

bool IRScreen::_tvbCancelCb() {
  if (!_activeInstance) return true;
  Uni.update();
  UartFM.poll();   // remote keyboard/mirror input — blocking loops must drain it
  LvglUi::pump();  // keep LVGL alive inside the blocking loop (renders progress)
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _activeInstance->_tvbCancelled = true;
      return true;
    }
  }
  return false;
}

// ── Quick Off / Find Code ─────────────────────────────────────────────────────

void IRScreen::_quickOff() {
  if (_txPin < 0) {
    ShowStatusAction::show("TX pin not set");
    render();
    return;
  }
  String spec = Config.get(APP_CONFIG_IR_QUICK_OFF, APP_CONFIG_IR_QUICK_OFF_DEFAULT);
  _ir.beginTx(_txPin);
  _tvbCancelled = false;
  _activeInstance = this;
  _state = STATE_POWERBLAST;
  ProgressView::init();
  const int sent = PowerBlast::instantOff(*_ir.sender(),
                                          spec.isEmpty() ? nullptr : spec.c_str(),
                                          _pbCancelCb, _pbProgressCb);
  _ir.end();
  ProgressView::finish();
  _activeInstance = nullptr;
  char msg[48];
  snprintf(msg, sizeof(msg), "%s: %d codes", spec.isEmpty() ? "net" : "saved+net", sent);
  ShowStatusAction::show(msg, 1500);
  _showMenu();
}

void IRScreen::_findCode() {
  if (_txPin < 0) {
    ShowStatusAction::show("TX pin not set");
    render();
    return;
  }
#ifdef BOARD_4MB
  // The universal sweep needs the 390 KB dump — not present on 4 MB boards.
  ShowStatusAction::show("Not available on 4MB build");
  render();
  return;
#else
  // Sweep the universal set in ~25-code bursts. PRESS during the run marks
  // "the TV just died" — the burst window (plus one code of margin on each
  // side) is saved as the Quick Off window and replayed instantly forever.
  const int CH = 25;
  _ir.beginTx(_txPin);
  _activeInstance = this;
  _tvbCancelled = false;
  _state = STATE_FIND_CODE;

  String found;
  int from = 0;
  while (from < PowerBlast::COUNT_ALL) {
    int to = from + CH;
    if (to > PowerBlast::COUNT_ALL) to = PowerBlast::COUNT_ALL;
    _tvbCancelled = false;
    ProgressView::init();
    char msg[48];
    snprintf(msg, sizeof(msg), "Burst %d-%d — PRESS = TV died", from + 1, to);
    ProgressView::progress(msg, from * 100 / PowerBlast::COUNT_ALL);

    for (int i = from; i < to; i++) {
      if (_pbCancelCb()) break;
      const PowerBlast::Code& c = PowerBlast::TABLE_ALL[i];
      PowerBlast::sendCode(*_ir.sender(), c.proto, c.address, c.command, c.bits);
      delay(50);
    }
    if (_tvbCancelled) {
      // окно найдено: [from-5 .. to]
      int lo = from - CH;
      if (lo < 0) lo = 0;
      found = _quickSpecBuild(lo, to);
      break;
    }
    from = to;
  }

  _ir.end();
  ProgressView::finish();
  _activeInstance = nullptr;
  _state = STATE_MENU;

  if (found.length()) {
    Config.set(APP_CONFIG_IR_QUICK_OFF, found);
    Config.save(Uni.Storage);
    ShowStatusAction::show("Saved! Quick Off is instant now", 2000);
    int n = Achievement.inc("ir_quick_off_saved");
    if (n == 1) Achievement.unlock("ir_quick_off_saved");
  } else if (from >= PowerBlast::COUNT_ALL) {
    ShowStatusAction::show("Sweep done, no PRESS caught", 2000);
  }
  _showMenu();
#endif  // !BOARD_4MB
}

String IRScreen::_quickSpecBuild(int from, int to) {
#ifdef BOARD_4MB
  (void)from; (void)to; return String();
#else
  String spec;
  for (int i = from; i < to && i < PowerBlast::COUNT_ALL; i++) {
    const PowerBlast::Code& c = PowerBlast::TABLE_ALL[i];
    char part[32];
    snprintf(part, sizeof(part), "%s:%X:%X:%d;", c.proto, c.address, c.command, c.bits);
    spec += part;
    if (spec.length() > 900) break;   // config value budget
  }
  return spec;
#endif  // !BOARD_4MB
}

// ── Power Blast callbacks ────────────────────────────────────────────────────

void IRScreen::_pbProgressCb(int current, int total) {
  if (!_activeInstance) return;
  // Throttle: one text update per ~150 ms — per-code updates churn LVGL's
  // label buffers thousands of times in a blocking loop with no pump, which
  // corrupted the widget heap (coredump: lv_obj_construct(obj=0x70006)).
  static uint32_t s_last = 0;
  uint32_t now = millis();
  if (now - s_last < 150) return;
  s_last = now;
  int pct = (int)((int32_t)current * 100 / total);
  char msg[40];
  snprintf(msg, sizeof(msg), "Blast %d / %d", current, total);
  ProgressView::progress(msg, pct);
}

bool IRScreen::_pbCancelCb() {
  if (!_activeInstance) return true;
  Uni.update();
  UartFM.poll();   // remote keyboard/mirror input — blocking loops must drain it
  LvglUi::pump();  // keep LVGL alive inside the blocking loop (renders progress)
  if (Uni.Nav->wasPressed()) {
    auto dir = Uni.Nav->readDirection();
    if (dir == INavigation::DIR_BACK || dir == INavigation::DIR_PRESS) {
      _activeInstance->_tvbCancelled = true;
      return true;
    }
  }
  return false;
}
