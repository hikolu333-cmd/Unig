#pragma once

#include "ui/templates/ListScreen.h"
#include "ui/views/BrowseFileView.h"

class WifiBeaconFloodScreen : public ListScreen
{
public:
  const char* title() override { return "Beacon Flood"; }
  bool inhibitPowerOff() override { return _state == STATE_ATTACKING; }

  ~WifiBeaconFloodScreen() override;

  void onInit() override;
  void onItemSelected(uint8_t index) override;
  void onUpdate() override;
  void onRender() override;
  void onBack() override;

private:
  enum State {
    STATE_MENU,
    STATE_COUNT,
    STATE_SCAN,
    STATE_SELECT_AP,
    STATE_FILE_PICK,
    STATE_ATTACKING
  };
  enum Mode       { MODE_SPAM, MODE_FLOOD };
  enum SpamTarget { SPAM_BUILTIN, SPAM_FILE };

  static constexpr int MAX_AP          = 20;
  static constexpr int MAX_FILE_SSIDS  = 200;
  static constexpr int kCountChoiceCount = 7;   // pool-size options (see .cpp)
  static constexpr const char* DICT_DIR = "/unigeek/wifi/beacon_dicts";

  struct ApEntry {
    char    ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int16_t rssi;
  };

  State      _state      = STATE_MENU;
  Mode       _mode       = MODE_SPAM;
  SpamTarget _spamTarget = SPAM_BUILTIN;

  ApEntry  _apList[MAX_AP]          = {};
  char     _apSubLabels[MAX_AP][18] = {};
  int      _apCount     = 0;
  int      _floodTarget = -1;
  int      _countIdx    = 6;   // index into kCountChoices; 6 = Endless

  class WifiAttackUtil* _attacker = nullptr;

  int      _ssidIdx     = 0;
  int      _poolNets    = 0;   // pool size of the running attack (for the Nets counter)
  int      _rounds      = 0;
  int      _sentThisSec = 0;
  int      _ratePerSec  = 0;
  uint32_t _lastRateTick = 0;
  uint32_t _lastSendMs   = 0;
  bool     _chromeDrawn  = false;

  // ── File-loaded SSIDs (SPAM_FILE) ────────────────────────────────────────
  char     _fileSsids[MAX_FILE_SSIDS][33] = {};
  int      _fileSsidCount = 0;
  String   _fileLabel;                              // basename for sublabel

  // ── File picker (BrowseFileView + virtual "Built In" entry at index 0) ─
  BrowseFileView _browser;
  ListItem       _pickerItems[BrowseFileView::kCap + 1] = {};
  String         _pickerDir;   // current directory in the file picker

  // Stable per-network BSSID base: randomized once per attack, index-suffixed
  // per pool slot. Phones track (SSID, BSSID) pairs — a churning BSSID makes
  // every cycle look like a brand-new AP, so entries age out of the scan list.
  uint8_t  _macBase[6] = {};
  char     _genSsidBuf[33] = {};   // rotated buffer for generated pool names

  String   _modeSub;
  String   _targetSub;
  String   _countSub;
  String   _startSub;
  // Spam menu: [Mode, Target, Count, Start]; Flood menu drops the Count row.
  ListItem _menuItems[4] = { {"Mode"}, {"Target"}, {"Count"}, {"Start"} };
  ListItem _countItems[kCountChoiceCount];
  ListItem _apItems[MAX_AP + 1];
  bool     _scanValid = false;

  void _updateMenuValues();
  void _showMenu();           // setItems + _updateMenuValues, right row count
  void _showCountPicker();
  const char* _poolSsid(int idx);   // built-in name, or a generated variant
  void _showFilePicker();
  bool _loadDictFile(const String& path);
  void _startScan(bool forceScan = false);
  void _showScanResults();
  void _startAttack();
  void _stop();
  void _broadcastNext();
  void _drawAttacking();
};
