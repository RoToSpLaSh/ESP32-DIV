#include <algorithm>
#include <vector>
#include "KeyboardUI.h"
#include "Touchscreen.h"
#include "config.h"
#include "icon.h"
#include "shared.h"


static void subghzQuietOtherSPIDevices() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, HIGH);
  pinMode(XPT2046_CS, OUTPUT);
  digitalWrite(XPT2046_CS, HIGH);

  pinMode(CSN_PIN_1, OUTPUT);
  pinMode(CE_PIN_1, OUTPUT);
  digitalWrite(CSN_PIN_1, HIGH);
  digitalWrite(CE_PIN_1, LOW);

  pinMode(CSN_PIN_2, OUTPUT);
  pinMode(CE_PIN_2, OUTPUT);
  digitalWrite(CSN_PIN_2, HIGH);
  digitalWrite(CE_PIN_2, LOW);

  pinMode(CSN_PIN_3, OUTPUT);
  pinMode(CE_PIN_3, OUTPUT);
  digitalWrite(CSN_PIN_3, HIGH);
  digitalWrite(CE_PIN_3, LOW);

  delay(3);
}

#include "SDCardManager.h"
#include "utils.h"



static void subghzRestoreSystemState() {
  ELECHOUSE_cc1101.setSidle();
  delay(2);
  subghzQuietOtherSPIDevices();
  SPI.end();
  delay(2);
  SPI.begin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, SD_CS);
  setStatusBarSdPollingEnabled(true);
  setStatusBarBatteryPollingEnabled(true);
  delay(8);
}

static void subghzPrepareSharedSPIBus() {
  subghzQuietOtherSPIDevices();
  SPI.end();
  delay(2);
  SPI.begin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_CS);
  delay(2);
}

static void subghzReleaseRadioForSD() {
  ELECHOUSE_cc1101.setSidle();
  delay(2);
  subghzQuietOtherSPIDevices();
  SPI.end();
  delay(2);
  SPI.begin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, SD_CS);
  delay(5);
}


static bool subghzPrepareSdForFileOps() {
  subghzReleaseRadioForSD();
  SDCardManager::invalidate();
  if (!SDCardManager::begin()) return false;
  return SDCardManager::ensureDir("/subghz");
}

static void subghzRadioInitCommon() {
  subghzPrepareSharedSPIBus();
  ELECHOUSE_cc1101.setSpiPin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_CS);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setGDO(CC1101_GDO0, CC1101_GDO2);
  ELECHOUSE_cc1101.setModulation(2);
  ELECHOUSE_cc1101.setPA(10);
  ELECHOUSE_cc1101.setRxBW(58.0);
  delay(5);
}



namespace {
  static constexpr const char* SUBGHZ_DIR = "/subghz";
  static constexpr const char* SUBGHZ_EXPORT_PREFIX = "/subghz/profiles_";
  static constexpr const char* SUBGHZ_CURRENT_PATH = "/subghz/profiles_current.bin";
  static constexpr uint32_t SUBGHZ_EXPORT_MAGIC = 0x315A4753;

  struct __attribute__((packed)) SubGhzProfile {
    uint32_t frequency;
    uint32_t value;
    uint16_t bitLength;
    uint16_t protocol;
    char     name[16];
  };

  static constexpr uint16_t MAX_NAME_LENGTH = 16;
  static constexpr uint16_t PROFILE_SIZE = sizeof(SubGhzProfile);

  static constexpr uint16_t ADDR_VALUE = 1280;
  static constexpr uint16_t ADDR_BITLEN = 1284;
  static constexpr uint16_t ADDR_PROTO = 1286;
  static constexpr uint16_t ADDR_FREQ = 1288;
  static constexpr uint16_t ADDR_PROFILE_COUNT = 1296;
  static constexpr uint16_t ADDR_PROFILE_START = 1300;
  static constexpr uint16_t MAX_PROFILES = 5;

  struct __attribute__((packed)) SubGhzExportHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint16_t profileSize;
    uint16_t reserved;
  };

  static bool subghzMountSD() {
  subghzReleaseRadioForSD();
  return SDCardManager::begin();
}

static bool subghzEnsureDir(const char* dirPath) {
  subghzReleaseRadioForSD();
  if (!SDCardManager::begin()) return false;
  return SDCardManager::ensureDir(dirPath);
}

  static void clearProfilesInEeprom() {

    uint16_t zero = 0;
    EEPROM.put(ADDR_PROFILE_COUNT, zero);
    SubGhzProfile empty{};
    for (uint16_t i = 0; i < MAX_PROFILES; i++) {
      EEPROM.put(ADDR_PROFILE_START + (i * PROFILE_SIZE), empty);
    }
    EEPROM.commit();
  }

  static bool makeNextExportPath(String& outPath) {

    for (uint16_t i = 0; i < 10000; i++) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s%04u.bin", SUBGHZ_EXPORT_PREFIX, (unsigned)i);
      if (!SD.exists(buf)) { outPath = String(buf); return true; }
    }
    return false;
  }

  static bool findLatestExportPath(String& outPath) {

    for (int i = 9999; i >= 0; i--) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s%04u.bin", SUBGHZ_EXPORT_PREFIX, (unsigned)i);
      if (SD.exists(buf)) { outPath = String(buf); return true; }
    }
    return false;
  }

  static bool exportProfilesToSD(String& outPath, String* errOut = nullptr) {
    subghzReleaseRadioForSD();
    if (!subghzEnsureDir(SUBGHZ_DIR)) {
      if (errOut) *errOut = "SD not mounted";
      return false;
    }

    uint16_t count = 0;
    EEPROM.get(ADDR_PROFILE_COUNT, count);
    if (count > MAX_PROFILES) count = MAX_PROFILES;

    if (!makeNextExportPath(outPath)) {
      if (errOut) *errOut = "No free filename";
      return false;
    }

    File f = SD.open(outPath.c_str(), FILE_WRITE);
    if (!f) {
      if (errOut) *errOut = "Open failed";
      return false;
    }

    SubGhzExportHeader h{};
    h.magic = SUBGHZ_EXPORT_MAGIC;
    h.version = 1;
    h.count = count;
    h.profileSize = PROFILE_SIZE;
    h.reserved = 0;

    bool ok = (f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h));
    for (uint16_t i = 0; ok && i < count; i++) {
      SubGhzProfile p{};
      int addr = ADDR_PROFILE_START + (i * PROFILE_SIZE);
      EEPROM.get(addr, p);
      ok = (f.write((const uint8_t*)&p, sizeof(p)) == sizeof(p));
    }
    f.close();

    if (!ok && errOut) *errOut = "Write failed";
    return ok;
  }

  static bool syncCurrentProfilesToSD(String* errOut = nullptr) {

    subghzReleaseRadioForSD();
    SDCardManager::invalidate();
    subghzReleaseRadioForSD();
    if (!subghzEnsureDir(SUBGHZ_DIR)) {
      if (errOut) *errOut = "SD not mounted";
      return false;
    }

    uint16_t count = 0;
    EEPROM.get(ADDR_PROFILE_COUNT, count);
    if (count > MAX_PROFILES) count = MAX_PROFILES;

    if (SD.exists(SUBGHZ_CURRENT_PATH)) SD.remove(SUBGHZ_CURRENT_PATH);
    File f = SD.open(SUBGHZ_CURRENT_PATH, FILE_WRITE);
    if (!f) {
      if (errOut) *errOut = "Open failed";
      return false;
    }

    SubGhzExportHeader h{};
    h.magic = SUBGHZ_EXPORT_MAGIC;
    h.version = 1;
    h.count = count;
    h.profileSize = PROFILE_SIZE;
    h.reserved = 0;

    bool ok = (f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h));
    for (uint16_t i = 0; ok && i < count; i++) {
      SubGhzProfile p{};
      int addr = ADDR_PROFILE_START + (i * PROFILE_SIZE);
      EEPROM.get(addr, p);
      ok = (f.write((const uint8_t*)&p, sizeof(p)) == sizeof(p));
    }
    f.close();
    if (!ok && errOut) *errOut = "Write failed";
    return ok;
  }

  static bool importProfilesFromSD(const String& path, String* errOut = nullptr) {
    if (!subghzMountSD()) {
      if (errOut) *errOut = "SD not mounted";
      return false;
    }
    if (path.isEmpty() || !SD.exists(path.c_str())) {
      if (errOut) *errOut = "File not found";
      return false;
    }

    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) {
      if (errOut) *errOut = "Open failed";
      return false;
    }

    SubGhzExportHeader h{};
    if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h)) { f.close(); if (errOut) *errOut="Bad header"; return false; }
    if (h.magic != SUBGHZ_EXPORT_MAGIC || h.version != 1) { f.close(); if (errOut) *errOut="Wrong file"; return false; }
    if (h.profileSize != PROFILE_SIZE) { f.close(); if (errOut) *errOut="Size mismatch"; return false; }

    uint16_t count = h.count;
    if (count > MAX_PROFILES) count = MAX_PROFILES;

    clearProfilesInEeprom();
    for (uint16_t i = 0; i < count; i++) {
      SubGhzProfile p{};
      if (f.read((uint8_t*)&p, sizeof(p)) != sizeof(p)) { f.close(); if (errOut) *errOut="Read failed"; return false; }
      p.name[MAX_NAME_LENGTH - 1] = '\0';
      EEPROM.put(ADDR_PROFILE_START + (i * PROFILE_SIZE), p);
    }
    EEPROM.put(ADDR_PROFILE_COUNT, count);
    EEPROM.commit();
    f.close();
    return true;
  }

  struct SubGhzFileEntry {
    String path;
    uint16_t count = 0;
    bool isCurrent = false;
  };

  static bool readExportHeader(File& f, SubGhzExportHeader& out, String* errOut = nullptr) {
    if (f.read((uint8_t*)&out, sizeof(out)) != sizeof(out)) { if (errOut) *errOut="Bad header"; return false; }
    if (out.magic != SUBGHZ_EXPORT_MAGIC || out.version != 1) { if (errOut) *errOut="Wrong file"; return false; }
    if (out.profileSize != PROFILE_SIZE) { if (errOut) *errOut="Size mismatch"; return false; }
    return true;
  }

  static bool readProfileAt(const String& path, uint16_t localIndex, SubGhzProfile& out, String* errOut = nullptr) {
    subghzReleaseRadioForSD();
    if (!subghzMountSD()) { if (errOut) *errOut="SD not mounted"; return false; }
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) { if (errOut) *errOut="Open failed"; return false; }
    SubGhzExportHeader h{};
    if (!readExportHeader(f, h, errOut)) { f.close(); return false; }
    uint16_t count = h.count; if (count > MAX_PROFILES) count = MAX_PROFILES;
    if (localIndex >= count) { f.close(); if (errOut) *errOut="Index OOR"; return false; }
    uint32_t off = (uint32_t)sizeof(SubGhzExportHeader) + (uint32_t)localIndex * (uint32_t)PROFILE_SIZE;
    if (!f.seek(off)) { f.close(); if (errOut) *errOut="Seek failed"; return false; }
    if (f.read((uint8_t*)&out, sizeof(out)) != sizeof(out)) { f.close(); if (errOut) *errOut="Read failed"; return false; }
    out.name[MAX_NAME_LENGTH - 1] = '\0';
    f.close();
    return true;
  }

  static bool listAllProfileFiles(std::vector<SubGhzFileEntry>& out, String* errOut = nullptr) {
    out.clear();
    subghzReleaseRadioForSD();
    if (!subghzMountSD()) { if (errOut) *errOut="SD not mounted"; return false; }
    if (!SD.exists(SUBGHZ_DIR)) { if (errOut) *errOut="No /subghz"; return false; }
    File d = SD.open(SUBGHZ_DIR);
    if (!d) { if (errOut) *errOut="Open dir failed"; return false; }

    for (;;) {
      File f = d.openNextFile();
      if (!f) break;
      if (f.isDirectory()) { f.close(); continue; }
      String name = String(f.name());

      String fullPath = String(SUBGHZ_DIR) + "/" + name;

      bool isCurrent = (name == "profiles_current.bin");
      bool isArchive = name.startsWith("profiles_") && name.endsWith(".bin") && !isCurrent;
      if (!isCurrent && !isArchive) { f.close(); continue; }

      SubGhzExportHeader h{};
      String herr;
      bool ok = readExportHeader(f, h, &herr);
      f.close();
      if (!ok) continue;

      uint16_t cnt = h.count;
      if (cnt > MAX_PROFILES) cnt = MAX_PROFILES;
      if (cnt == 0) continue;

      SubGhzFileEntry e;
      e.path = fullPath;
      e.count = cnt;
      e.isCurrent = isCurrent;
      out.push_back(e);
    }
    d.close();

    std::sort(out.begin(), out.end(), [](const SubGhzFileEntry& a, const SubGhzFileEntry& b) {
      if (a.isCurrent != b.isCurrent) return a.isCurrent > b.isCurrent;
      return a.path > b.path;
    });
    return true;
  }

  static uint16_t totalProfilesInIndex(const std::vector<SubGhzFileEntry>& files) {
    uint32_t total = 0;
    for (auto& f : files) total += f.count;
    if (total > 65535) total = 65535;
    return (uint16_t)total;
  }

  static bool locateGlobalIndex(const std::vector<SubGhzFileEntry>& files, uint16_t globalIndex,
                                String& outPath, uint16_t& outLocalIdx) {
    uint32_t idx = globalIndex;
    for (auto& fe : files) {
      if (idx < fe.count) {
        outPath = fe.path;
        outLocalIdx = (uint16_t)idx;
        return true;
      }
      idx -= fe.count;
    }
    return false;
  }

  static bool deleteProfileFromFile(const String& path, uint16_t localIndex, String* errOut = nullptr) {
    subghzReleaseRadioForSD();
    if (!subghzMountSD()) { if (errOut) *errOut="SD not mounted"; return false; }
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) { if (errOut) *errOut="Open failed"; return false; }
    SubGhzExportHeader h{};
    if (!readExportHeader(f, h, errOut)) { f.close(); return false; }
    uint16_t count = h.count; if (count > MAX_PROFILES) count = MAX_PROFILES;
    if (localIndex >= count) { f.close(); if (errOut) *errOut="Index OOR"; return false; }

    SubGhzProfile buf[MAX_PROFILES]{};
    for (uint16_t i = 0; i < count; i++) {
      if (f.read((uint8_t*)&buf[i], sizeof(SubGhzProfile)) != sizeof(SubGhzProfile)) { f.close(); if (errOut) *errOut="Read failed"; return false; }
      buf[i].name[MAX_NAME_LENGTH - 1] = '\0';
    }
    f.close();

    for (uint16_t i = localIndex; i + 1 < count; i++) buf[i] = buf[i + 1];
    count--;

    if (SD.exists(path.c_str())) SD.remove(path.c_str());
    File w = SD.open(path.c_str(), FILE_WRITE);
    if (!w) { if (errOut) *errOut="Open write failed"; return false; }

    SubGhzExportHeader nh{};
    nh.magic = SUBGHZ_EXPORT_MAGIC;
    nh.version = 1;
    nh.count = count;
    nh.profileSize = PROFILE_SIZE;
    nh.reserved = 0;
    bool ok = (w.write((const uint8_t*)&nh, sizeof(nh)) == sizeof(nh));
    for (uint16_t i = 0; ok && i < count; i++) {
      ok = (w.write((const uint8_t*)&buf[i], sizeof(SubGhzProfile)) == sizeof(SubGhzProfile));
    }
    w.close();
    if (!ok && errOut) *errOut="Write failed";

    if (ok && path.endsWith("profiles_current.bin")) {
      importProfilesFromSD(path, nullptr);
    }
    return ok;
  }
}

#ifdef TFT_BLACK
#undef TFT_BLACK
#endif
#define TFT_BLACK FEATURE_BG

#ifndef FEATURE_TEXT
#define FEATURE_TEXT STATUS_ICON
#endif
#ifndef FEATURE_WHITE
#define FEATURE_WHITE 0xFFFF
#endif

#ifdef TFT_WHITE
#undef TFT_WHITE
#endif
#define TFT_WHITE FEATURE_TEXT

#ifdef WHITE
#undef WHITE
#endif
#define WHITE FEATURE_WHITE

#ifdef DARK_GRAY
#undef DARK_GRAY
#endif
#define DARK_GRAY UI_FG

namespace replayat {

#define EEPROM_SIZE 4096

#define ADDR_VALUE         1280
#define ADDR_BITLEN        1284
#define ADDR_PROTO         1286
#define ADDR_FREQ          1288
#define ADDR_PROFILE_COUNT 1296
#define ADDR_PROFILE_START 1300
#define MAX_PROFILES       32

#define SCREEN_WIDTH  240
#define SCREENHEIGHT 320
#define SCREEN_HEIGHT 320

static bool uiDrawn = false;

#define MAX_NAME_LENGTH 16

const char* profileKeyboardRows[] = {
  "1234567890",
  "QWERTYUIOP",
  "ASDFGHJKL",
  "ZXCVBNM<-"
};

const char* randomNames[] = {
  "Signal", "Remote", "KeyFob", "GateOpener", "DoorLock",
  "RFTest", "Profile", "Control", "Switch", "Beacon"
};
const uint8_t numRandomNames = 10;

struct __attribute__((packed)) Profile {
    uint32_t frequency;
    uint32_t value;
    uint16_t bitLength;
    uint16_t protocol;
    char name[MAX_NAME_LENGTH];
};

#define PROFILE_SIZE sizeof(Profile)

uint16_t profileCount = 0;

RCSwitch mySwitch = RCSwitch();
arduinoFFT FFTSUB = arduinoFFT();

const uint16_t samplesSUB = 256;
const double FrequencySUB = 5000;

double attenuation_num = 10;

unsigned int sampling_period;
unsigned long micro_s;

double vRealSUB[samplesSUB];
double vImagSUB[samplesSUB];

byte red[128], green[128], blue[128];

unsigned int epochSUB = 0;
unsigned int colorcursor = 2016;

int rssi;

static constexpr uint8_t REPLAY_RX_PIN = SUBGHZ_RX_PIN;
static constexpr uint8_t REPLAY_TX_PIN = SUBGHZ_TX_PIN;

uint32_t receivedValue = 0;
uint16_t receivedBitLength = 0;
uint16_t receivedProtocol = 0;
const int rssi_threshold = -75;

static const uint32_t subghz_frequency_list[] = {
    300000000, 303875000, 304250000, 310000000, 315000000, 318000000,
    390000000, 418000000, 433075000, 433420000, 433920000, 434420000,
    434775000, 438900000, 868350000, 915000000, 925000000
};

uint16_t currentFrequencyIndex = 0;
int yshift = 20;

static bool autoScanEnabled = false;
static uint16_t scanIndex = 0;
static uint32_t lastHopMs = 0;
static uint32_t lockUntilMs = 0;
static constexpr uint32_t SCAN_DWELL_MS = 110;
static constexpr uint32_t LOCK_HOLD_MS  = 2500;
static constexpr uint32_t RSSI_LOCK_MS  = 1200;
static constexpr int      RSSI_DETECT_THRESHOLD = -72;
static constexpr int      RSSI_CLEAR_THRESHOLD  = -78;
static constexpr uint32_t UI_SCAN_UPDATE_MS = 250;
static uint32_t lastUiScanUpdateMs = 0;
static bool     rssiHot = false;

static uint32_t lastDetectAlertMs = 0;
static uint16_t lastDetectAlertFreq = 0xFFFF;
static uint32_t notifHideAtMs = 0;
static bool notifActive = false;

static constexpr uint8_t BUZZER_LEDC_CH = 7;
static bool buzzerArmed = false;
static uint32_t buzzerOffAtMs = 0;
static void replayBeep(uint16_t hz = 2200, uint16_t ms = 60) {
  #ifdef BUZZER_PIN
  ledcSetup(BUZZER_LEDC_CH, 4000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);
  ledcWriteTone(BUZZER_LEDC_CH, hz);
  buzzerArmed = true;
  buzzerOffAtMs = millis() + ms;
  #endif
}

static void replayBeepPoll() {
  #ifdef BUZZER_PIN
  if (!buzzerArmed) return;
  if ((int32_t)(millis() - buzzerOffAtMs) < 0) return;
  ledcWriteTone(BUZZER_LEDC_CH, 0);

  ledcDetachPin(BUZZER_PIN);
  buzzerArmed = false;
  #endif
}

static void replayShowDetectNotice(const String& reason, int rssi = 0) {
  uint32_t now = millis();

  if (now - lastDetectAlertMs < 1200 && lastDetectAlertFreq == currentFrequencyIndex) return;
  lastDetectAlertMs = now;
  lastDetectAlertFreq = currentFrequencyIndex;

  char msg[96];
  float mhz = subghz_frequency_list[currentFrequencyIndex] / 1000000.0f;

  snprintf(msg, sizeof(msg), "%s @ %.2f MHz | RSSI %d", reason.c_str(), mhz, rssi);
  showNotificationActions("SubGHz Detected", msg, true);
  replayBeep(reason == "DECODE" ? 2600 : 2000, 70);
  notifActive = true;
  notifHideAtMs = 0;
}

static inline uint16_t freqCount() {
  return (uint16_t)(sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
}

static void tuneToIndex(uint16_t idx, bool persist = true) {
  currentFrequencyIndex = idx % freqCount();
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(subghz_frequency_list[currentFrequencyIndex] / 1000000.0);
  ELECHOUSE_cc1101.SetRx();
  if (persist) {
    EEPROM.put(ADDR_FREQ, currentFrequencyIndex);
    EEPROM.commit();
  }
}

void updateDisplay() {
    uiDrawn = false;

    tft.fillRect(0, 40, 240, 40, TFT_BLACK);
    tft.drawLine(0, 80, 240, 80, TFT_WHITE);

    tft.setCursor(5, 20 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("Freq:");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(50, 20 + yshift);
    tft.print(subghz_frequency_list[currentFrequencyIndex] / 1000000.0, 2);
    tft.print(" MHz");

    tft.setCursor(175, 20 + yshift);
    bool locked = (autoScanEnabled && lockUntilMs != 0 && (int32_t)(millis() - lockUntilMs) < 0);
    tft.setTextColor(autoScanEnabled ? STATUS_ICON : TFT_WHITE);
    if (locked) {
      tft.print("LOCK");
    } else {
      tft.print(autoScanEnabled ? "AUTO" : "MAN ");
    }

    tft.setCursor(5, 35 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("Bit:");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(50, 35 + yshift);
    tft.printf("%d", receivedBitLength);

    tft.setCursor(130, 35 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("RSSI:");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(170, 35 + yshift);
    tft.printf("%d", ELECHOUSE_cc1101.getRssi());

    tft.setCursor(130, 20 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("Ptc:");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(170, 20 + yshift);
    tft.printf("%d", receivedProtocol);

    tft.setCursor(5, 50 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("Val:");
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(50, 50 + yshift);
    tft.print(receivedValue);

}

String getUserInputName() {
  OnScreenKeyboardConfig cfg;
  cfg.titleLine1     = "[!] Set a name for the saved profile.";
  cfg.titleLine2     = "(max 15 chars)";
  cfg.rows           = profileKeyboardRows;
  cfg.rowCount       = 4;
  cfg.maxLen         = MAX_NAME_LENGTH - 1;
  cfg.shuffleNames   = randomNames;
  cfg.shuffleCount   = numRandomNames;
  cfg.buttonsY       = 195;
  cfg.backLabel      = "Back";
  cfg.middleLabel    = "Shuffle";
  cfg.okLabel        = "OK";
  cfg.enableShuffle  = true;
  cfg.requireNonEmpty = true;
  cfg.emptyErrorMsg  = "Name cannot be empty!";

  OnScreenKeyboardResult r = showOnScreenKeyboard(cfg, "");

  if (!r.accepted) {

    tft.fillScreen(TFT_BLACK);
    updateDisplay();
  }
  return r.text;
}

void sendSignal() {

    replayat::mySwitch.disableReceive();
    delay(20);
    replayat::mySwitch.enableTransmit(REPLAY_TX_PIN);
    ELECHOUSE_cc1101.SetTx();

    tft.fillRect(0,40,240,37, TFT_BLACK);

    tft.setCursor(10, 30 + yshift);
    tft.print("Sending...");
    tft.setCursor(10, 40 + yshift);
    tft.print(receivedValue);

    mySwitch.setProtocol(receivedProtocol);
    replayat::mySwitch.send(receivedValue, receivedBitLength);

    delay(150);
    tft.fillRect(0,40,240,37, TFT_BLACK);
    tft.setCursor(10, 30 + yshift);
    tft.print("Done!");

    replayat::mySwitch.disableTransmit();
    delay(20);
    ELECHOUSE_cc1101.SetRx();
    replayat::mySwitch.enableReceive(REPLAY_RX_PIN);

    delay(150);
    updateDisplay();
}

void do_sampling() {

  micro_s = micros();

  #define ALPHA 0.2
  float ewmaRSSI = -50;

for (int i = 0; i < samplesSUB; i++) {
    int rssi = ELECHOUSE_cc1101.getRssi();
    rssi += 100;

    ewmaRSSI = (ALPHA * rssi) + ((1 - ALPHA) * ewmaRSSI);

    vRealSUB[i] = ewmaRSSI * 2;
    vImagSUB[i] = 1;

    while (micros() < micro_s + sampling_period);
    micro_s += sampling_period;
}

  double mean = 0;

  for (uint16_t i = 0; i < samplesSUB; i++)
        mean += vRealSUB[i];
        mean /= samplesSUB;
  for (uint16_t i = 0; i < samplesSUB; i++)
        vRealSUB[i] -= mean;

  micro_s = micros();

  FFTSUB.Windowing(vRealSUB, samplesSUB, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFTSUB.Compute(vRealSUB, vImagSUB, samplesSUB, FFT_FORWARD);
  FFTSUB.ComplexToMagnitude(vRealSUB, vImagSUB, samplesSUB);

unsigned int left_x = 120;
unsigned int graph_y_offset = 81;
int max_k = 0;

for (int j = 0; j < samplesSUB >> 1; j++) {
    int k = vRealSUB[j] / attenuation_num;
    if (k > max_k)
        max_k = k;
    if (k > 127) k = 127;

    unsigned int color = red[k] << 11 | green[k] << 5 | blue[k];
    unsigned int vertical_x = left_x + j;

    tft.drawPixel(vertical_x, epochSUB + graph_y_offset, color);
}

for (int j = 0; j < samplesSUB >> 1; j++) {
    int k = vRealSUB[j] / attenuation_num;
    if (k > max_k)
        max_k = k;
    if (k > 127) k = 127;

    unsigned int color = red[k] << 11 | green[k] << 5 | blue[k];
    unsigned int mirrored_x = left_x - j;
    tft.drawPixel(mirrored_x, epochSUB + graph_y_offset, color);
}

  double tattenuation = max_k / 127.0;

  if (tattenuation > attenuation_num)
    attenuation_num = tattenuation;

    delay(10);
}

void readProfileCount() {
    EEPROM.get(ADDR_PROFILE_COUNT, profileCount);
    if (profileCount > MAX_PROFILES) profileCount = 0;
}

void saveProfile() {
    readProfileCount();

    String customName = getUserInputName();
    if (customName.length() == 0) {
        subghzRadioInitCommon();
        tuneToIndex(currentFrequencyIndex, false);
        replayat::mySwitch.enableReceive(REPLAY_RX_PIN);
        replayat::mySwitch.setRepeatTransmit(8);
        tft.setRotation(2);
        tft.fillScreen(TFT_BLACK);
        float currentBatteryVoltage = readBatteryVoltage();
        drawStatusBar(currentBatteryVoltage, true);
        updateDisplay();
        uiDrawn = false;
        subghzRestoreSystemState();
        return;
    }

    if (profileCount >= MAX_PROFILES) {
        tft.fillScreen(TFT_BLACK);
        tft.setRotation(2);
        tft.setCursor(10, 30 + yshift);
        tft.setTextColor(UI_WARN);
        tft.print("Storage full!");
        tft.setCursor(10, 45 + yshift);
        tft.setTextColor(TFT_WHITE);
        tft.print("Max EEPROM profiles reached");
        delay(1200);

        subghzRadioInitCommon();
        tuneToIndex(currentFrequencyIndex, false);
        replayat::mySwitch.enableReceive(REPLAY_RX_PIN);
        replayat::mySwitch.setRepeatTransmit(8);

        tft.fillScreen(TFT_BLACK);
        currentBatteryVoltage = readBatteryVoltage();
        drawStatusBar(currentBatteryVoltage, true);
        updateDisplay();
        uiDrawn = false;
        subghzRestoreSystemState();
        return;
    }

    Profile newProfile;
    newProfile.frequency = subghz_frequency_list[currentFrequencyIndex];
    newProfile.value = (uint32_t)receivedValue;
    newProfile.bitLength = (uint16_t)receivedBitLength;
    newProfile.protocol = (uint16_t)receivedProtocol;
    strncpy(newProfile.name, customName.c_str(), MAX_NAME_LENGTH - 1);
    newProfile.name[MAX_NAME_LENGTH - 1] = '\0';

    int addr = ADDR_PROFILE_START + (profileCount * PROFILE_SIZE);
    EEPROM.put(addr, newProfile);
    profileCount++;
    EEPROM.put(ADDR_PROFILE_COUNT, profileCount);
    EEPROM.commit();

    tft.fillScreen(TFT_BLACK);
    tft.setRotation(2);
    tft.setCursor(10, 30 + yshift);
    tft.setTextColor(TFT_WHITE);
    tft.print("Profile saved!");
    tft.setCursor(10, 40 + yshift);
    tft.print("Name: ");
    tft.print(newProfile.name);
    tft.setCursor(10, 50 + yshift);
    tft.print("Profiles saved: ");
    tft.println(profileCount);

    delay(800);

    subghzRadioInitCommon();
    tuneToIndex(currentFrequencyIndex, false);
    replayat::mySwitch.enableReceive(REPLAY_RX_PIN);
    replayat::mySwitch.setRepeatTransmit(8);

    tft.fillScreen(TFT_BLACK);
    currentBatteryVoltage = readBatteryVoltage();
    drawStatusBar(currentBatteryVoltage, true);
    updateDisplay();
    uiDrawn = false;
}

void loadProfileCount() {

    readProfileCount();
}

void runUI() {

    #define STATUS_BAR_Y_OFFSET 20
    #define STATUS_BAR_HEIGHT 16
    #define ICON_SIZE 16
    #define ICON_NUM 6

    static int iconX[ICON_NUM] = {90, 130, 170, 210, 50, 10};
    static int iconY = STATUS_BAR_Y_OFFSET;

    static const unsigned char* icons[ICON_NUM] = {
        bitmap_icon_sort_up_plus,
        bitmap_icon_sort_down_minus,
        bitmap_icon_antenna,
        bitmap_icon_floppy,
        bitmap_icon_random,
        bitmap_icon_go_back
    };

    if (!uiDrawn) {
        tft.drawLine(0, 19, 240, 19, TFT_WHITE);
        tft.fillRect(0, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

        for (int i = 0; i < ICON_NUM; i++) {
            if (icons[i] != NULL) {
                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
            }
        }
        tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, STATUS_ICON);
        uiDrawn = true;
    }

    static unsigned long lastAnimationTime = 0;
    static int animationState = 0;
    static int activeIcon = -1;

    if (animationState > 0 && millis() - lastAnimationTime >= 150) {
        if (animationState == 1) {
            tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
            animationState = 2;

            switch (activeIcon) {
                case 0:
                    autoScanEnabled = false;
                    currentFrequencyIndex = (currentFrequencyIndex + 1) % (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
                    tuneToIndex(currentFrequencyIndex, true);
                    updateDisplay();
                    break;
                case 1:
                    autoScanEnabled = false;
                    currentFrequencyIndex = (currentFrequencyIndex - 1 + (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]))) % (sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
                    tuneToIndex(currentFrequencyIndex, true);
                    updateDisplay();
                    break;
                case 2:
                    sendSignal();
                    break;
                case 3:
                    saveProfile();
                    break;
                case 4:
                    autoScanEnabled = !autoScanEnabled;
                    scanIndex = currentFrequencyIndex;
                    lastHopMs = 0;
                    lockUntilMs = 0;
                    lastUiScanUpdateMs = 0;
                    rssiHot = false;
                    updateDisplay();
                    break;
            }
        } else if (animationState == 2) {
            animationState = 0;
            activeIcon = -1;
        }
        lastAnimationTime = millis();
    }

    static unsigned long lastTouchCheck = 0;
    const unsigned long touchCheckInterval = 50;

    if (millis() - lastTouchCheck >= touchCheckInterval) {
        int x, y;
        if (feature_active && readTouchXY(x, y)) {
            if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
                for (int i = 0; i < ICON_NUM; i++) {
                    if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
                        if (icons[i] != NULL && animationState == 0) {

                            if (i == 5) {
                                feature_exit_requested = true;
                            } else {

                                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
                                animationState = 1;
                                activeIcon = i;
                                lastAnimationTime = millis();
                            }
                        }
                        break;
                    }
                }
            }
        }
        lastTouchCheck = millis();
    }
}

void ReplayAttackSetup() {
    setStatusBarSdPollingEnabled(false);
    setStatusBarBatteryPollingEnabled(false);

  subghzRadioInitCommon();
  EEPROM.begin(EEPROM_SIZE);
  readProfileCount();

  EEPROM.get(ADDR_VALUE, receivedValue);
  EEPROM.get(ADDR_BITLEN, receivedBitLength);
  EEPROM.get(ADDR_PROTO, receivedProtocol);
  EEPROM.get(ADDR_FREQ, currentFrequencyIndex);

  const uint16_t count = (uint16_t)(sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]));
  if (currentFrequencyIndex >= count) currentFrequencyIndex = 0;

  tuneToIndex(currentFrequencyIndex, false);

  replayat::mySwitch.enableReceive(REPLAY_RX_PIN);
  replayat::mySwitch.setRepeatTransmit(8);

  tft.fillScreen(TFT_BLACK);
  tft.setRotation(2);

  pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
  pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
  pcf.pinMode(BTN_UP, INPUT_PULLUP);
  pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
  pcf.pinMode(BTN_SELECT, INPUT_PULLUP);

  sampling_period = round(1000000*(1.0/FrequencySUB));

  for (int i = 0; i < 32; i++) { red[i] = i / 2; green[i] = 0; blue[i] = i; }
  for (int i = 32; i < 64; i++) { red[i] = i / 2; green[i] = 0; blue[i] = 63 - i; }
  for (int i = 64; i < 96; i++) { red[i] = 31; green[i] = (i - 64) * 2; blue[i] = 0; }
  for (int i = 96; i < 128; i++) { red[i] = 31; green[i] = 63; blue[i] = i - 96; }

  drawStatusBar(currentBatteryVoltage, true);
  updateDisplay();
  uiDrawn = false;
}

void ReplayAttackLoop() {

    if (feature_active && isButtonPressed(BTN_SELECT)) {
        replayat::mySwitch.disableReceive();
        replayat::mySwitch.disableTransmit();
        delay(5);
        ELECHOUSE_cc1101.setSidle();
        digitalWrite(SUBGHZ_TX_PIN, LOW);
        setStatusBarSdPollingEnabled(true);
        setStatusBarBatteryPollingEnabled(true);
        subghzRestoreSystemState();
        feature_exit_requested = true;
        return;
    }

    runUI();

    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 200;

    static bool prevLeft = false, prevRight = false, prevUp = false, prevDown = false;
    const bool leftPressed  = isButtonPressed(BTN_LEFT);
    const bool rightPressed = isButtonPressed(BTN_RIGHT);
    const bool upPressed    = isButtonPressed(BTN_UP);
    const bool downPressed  = isButtonPressed(BTN_DOWN);

    replayBeepPoll();

    if (notifActive && isNotificationVisible()) {
      int x, y;
      if (readTouchXY(x, y)) {
        NotificationAction act = notificationHandleTouch(x, y);
        if (act == NotificationAction::Save) {
          notifActive = false;

          tft.fillScreen(TFT_BLACK);
          uiDrawn = false;
          float v = readBatteryVoltage();
          drawStatusBar(v, true);
          runUI();
          updateDisplay();

          autoScanEnabled = false;
          saveProfile();

          tft.fillScreen(TFT_BLACK);
          uiDrawn = false;
          v = readBatteryVoltage();
          drawStatusBar(v, true);
          runUI();
          updateDisplay();
        } else if (act == NotificationAction::Ok || act == NotificationAction::Close) {
          notifActive = false;

          lastDetectAlertMs = millis();
          lastDetectAlertFreq = currentFrequencyIndex;
          lockUntilMs = millis() + 1500;
          rssiHot = true;

          tft.fillScreen(TFT_BLACK);
          uiDrawn = false;
          float v = readBatteryVoltage();
          drawStatusBar(v, true);
          runUI();
          updateDisplay();
        }
      }

      return;
    } else if (notifActive && !isNotificationVisible()) {

      notifActive = false;
      tft.fillScreen(TFT_BLACK);
      uiDrawn = false;
      float v = readBatteryVoltage();
      drawStatusBar(v, true);
      runUI();
      updateDisplay();
    }

    if (rightPressed && !prevRight && millis() - lastDebounceTime > debounceDelay) {
        autoScanEnabled = false;
        lockUntilMs = 0;
        rssiHot = false;
        tuneToIndex((uint16_t)((currentFrequencyIndex + 1) % freqCount()), true);
        updateDisplay();
        lastDebounceTime = millis();
    }
    if (leftPressed && !prevLeft && millis() - lastDebounceTime > debounceDelay) {
        autoScanEnabled = false;
        lockUntilMs = 0;
        rssiHot = false;
        tuneToIndex((uint16_t)((currentFrequencyIndex + freqCount() - 1) % freqCount()), true);
        updateDisplay();
        lastDebounceTime = millis();
    }
    if (upPressed && !prevUp && receivedValue != 0 && millis() - lastDebounceTime > debounceDelay) {

        autoScanEnabled = false;
        lockUntilMs = 0;
        rssiHot = false;
        sendSignal();
        lastDebounceTime = millis();
    }
    if (downPressed && !prevDown && millis() - lastDebounceTime > debounceDelay) {

        autoScanEnabled = !autoScanEnabled;
        scanIndex = currentFrequencyIndex;
        lastHopMs = 0;
        lockUntilMs = 0;
        lastUiScanUpdateMs = 0;
        rssiHot = false;
        updateDisplay();
        lastDebounceTime = millis();
    }

    prevLeft = leftPressed;
    prevRight = rightPressed;
    prevUp = upPressed;
    prevDown = downPressed;

    if (autoScanEnabled) {
      uint32_t now = millis();
      if (lockUntilMs != 0 && (int32_t)(now - lockUntilMs) < 0) {

      } else {

        if (lastHopMs == 0 || (now - lastHopMs) >= SCAN_DWELL_MS) {
          scanIndex = (uint16_t)((scanIndex + 1) % freqCount());

          tuneToIndex(scanIndex, false);
          lastHopMs = now;

          int rssi = ELECHOUSE_cc1101.getRssi();
          if (!rssiHot && rssi > RSSI_DETECT_THRESHOLD) {
            rssiHot = true;
            lockUntilMs = now + RSSI_LOCK_MS;

            EEPROM.put(ADDR_FREQ, currentFrequencyIndex);
            EEPROM.commit();
            replayShowDetectNotice("RSSI", rssi);
          } else if (rssiHot && rssi < RSSI_CLEAR_THRESHOLD) {
            rssiHot = false;
          }

          if (lastUiScanUpdateMs == 0 || (now - lastUiScanUpdateMs) >= UI_SCAN_UPDATE_MS) {
            updateDisplay();
            lastUiScanUpdateMs = now;
          }
        }
      }
    }

    do_sampling();
    delay(10);
    epochSUB++;

    if (epochSUB >= tft.width())
      epochSUB = 0;

    if (mySwitch.available()) {
        receivedValue = mySwitch.getReceivedValue();
        receivedBitLength = mySwitch.getReceivedBitlength();
        receivedProtocol = mySwitch.getReceivedProtocol();

        EEPROM.put(ADDR_VALUE, receivedValue);
        EEPROM.put(ADDR_BITLEN, receivedBitLength);
        EEPROM.put(ADDR_PROTO, receivedProtocol);
        EEPROM.commit();

        updateDisplay();

        if (autoScanEnabled) {
          lockUntilMs = millis() + LOCK_HOLD_MS;
          scanIndex = currentFrequencyIndex;
          rssiHot = false;

          EEPROM.put(ADDR_FREQ, currentFrequencyIndex);
          EEPROM.commit();
          replayShowDetectNotice("DECODE", ELECHOUSE_cc1101.getRssi());
        }
        mySwitch.resetAvailable();
    }

  }
}

namespace SavedProfile {

static bool uiDrawn = false;

#define EEPROM_SIZE 4096

#define ADDR_PROFILE_COUNT 1296
#define ADDR_PROFILE_START 1300
#define MAX_PROFILES       32
#define MAX_NAME_LENGTH    16

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

RCSwitch mySwitch = RCSwitch();
struct __attribute__((packed)) Profile {
    uint32_t frequency;
    uint32_t value;
    uint16_t bitLength;
    uint16_t protocol;
    char name[MAX_NAME_LENGTH];
};

#define PROFILE_SIZE sizeof(Profile)

uint16_t profileCount = 0;
uint16_t currentProfileIndex = 0;

int yshift = 16;

static std::vector<SubGhzFileEntry> sdFiles;
static uint16_t sdTotalProfiles = 0;
static String sdLastErr = "";
static String selectedPath = "";
static uint16_t selectedLocalIdx = 0;
static Profile selectedProfile{};
static bool selectedValid = false;

static constexpr uint8_t ITEMS_PER_PAGE = 7;
static constexpr int LIST_X = 6;
static constexpr int LIST_W = 228;

static constexpr int LIST_Y = 64;
static constexpr int ROW_H  = 18;
static constexpr int BOT_H = 32;
static constexpr int BOT_Y = 320 - BOT_H;

static constexpr int UI_GAP_Y = 6;
static constexpr int DETAILS_H = (BOT_Y - (LIST_Y + (ITEMS_PER_PAGE * ROW_H)) - (2 * UI_GAP_Y));
static constexpr int DETAILS_Y = BOT_Y - UI_GAP_Y - DETAILS_H;
static constexpr int BOT_GAP = 8;
static constexpr int BOT_BTN_W = (240 - 10 - 10 - BOT_GAP) / 2;
static constexpr int BOT_BTN_H = 24;
static constexpr int BOT_BTN_Y = BOT_Y + 4;
static constexpr int BOT_TX_X  = 10;
static constexpr int BOT_DEL_X = BOT_TX_X + BOT_BTN_W + BOT_GAP;

static uint16_t cachedPageStart = 0xFFFF;
static SubGhzProfile cachedPage[ITEMS_PER_PAGE]{};
static bool cachedOk[ITEMS_PER_PAGE]{};
static bool cacheDirty = true;

static bool deleteArmed = false;
static uint32_t deleteArmUntilMs = 0;

static void drawBottomButtons() {

  tft.fillRect(0, BOT_Y, 240, BOT_H, TFT_BLACK);

  FeatureUI::drawButtonRect(BOT_TX_X, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H,
                            "Transmit", FeatureUI::ButtonStyle::Primary);

  const bool armed = deleteArmed && (int32_t)(millis() - deleteArmUntilMs) < 0;
  const char* delLabel = armed ? "Delete?" : "Delete";
  FeatureUI::drawButtonRect(BOT_DEL_X, BOT_BTN_Y, BOT_BTN_W, BOT_BTN_H,
                            delLabel, FeatureUI::ButtonStyle::Danger);
}

static void refreshSdIndex(bool keepSelection = true) {
    uint16_t oldIdx = currentProfileIndex;
    EEPROM.get(ADDR_PROFILE_COUNT, profileCount);
    if (profileCount > MAX_PROFILES) profileCount = MAX_PROFILES;
    sdTotalProfiles = profileCount;
    sdLastErr = "";
    if (sdTotalProfiles == 0) {
        currentProfileIndex = 0;
        selectedValid = false;
        sdLastErr = "No profiles found";
        cacheDirty = true;
        return;
    }
    if (keepSelection) currentProfileIndex = oldIdx;
    if (currentProfileIndex >= sdTotalProfiles) currentProfileIndex = (uint16_t)(sdTotalProfiles - 1);
    selectedValid = false;
    cacheDirty = true;
}

static bool loadSelectedFromSd(String* errOut = nullptr) {
    if (sdTotalProfiles == 0) {
        selectedValid = false;
        if (errOut) *errOut = "No profiles";
        return false;
    }

    int addr = ADDR_PROFILE_START + (currentProfileIndex * PROFILE_SIZE);
    EEPROM.get(addr, selectedProfile);
    selectedProfile.name[MAX_NAME_LENGTH - 1] = ' ';

    selectedPath = "EEPROM";
    selectedLocalIdx = currentProfileIndex;
    selectedValid = true;
    if (errOut) *errOut = "";
    return true;
}

static uint16_t pageStartForIndex(uint16_t idx) {
  return (uint16_t)((idx / ITEMS_PER_PAGE) * ITEMS_PER_PAGE);
}

static void ensurePageCache() {
  if (sdTotalProfiles == 0) return;
  uint16_t start = pageStartForIndex(currentProfileIndex);
  if (!cacheDirty && cachedPageStart == start) return;
  cachedPageStart = start;
  for (uint8_t i = 0; i < ITEMS_PER_PAGE; i++) {
    cachedOk[i] = false;
    uint16_t globalIdx = (uint16_t)(start + i);
    if (globalIdx >= sdTotalProfiles) continue;
    int addr = ADDR_PROFILE_START + (globalIdx * PROFILE_SIZE);
    Profile p{};
    EEPROM.get(addr, p);

    cachedPage[i].frequency = p.frequency;
    cachedPage[i].value = p.value;
    cachedPage[i].bitLength = p.bitLength;
    cachedPage[i].protocol = p.protocol;
    memcpy(cachedPage[i].name, p.name, MAX_NAME_LENGTH);
    cachedPage[i].name[MAX_NAME_LENGTH - 1] = ' ';
    cachedOk[i] = true;
  }
  cacheDirty = false;
}

static void drawHeaderLine() {

  int hy = 30 + yshift;
  tft.fillRect(0, hy, 240, 14, TFT_BLACK);
  tft.setCursor(10, hy);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.printf("Profile %d/%d", (int)currentProfileIndex + 1, (int)sdTotalProfiles);
}

static void drawRow(uint16_t pageStart, uint8_t row) {
  uint16_t globalIdx = (uint16_t)(pageStart + row);
  if (globalIdx >= sdTotalProfiles) return;

  bool isSel = (globalIdx == currentProfileIndex);
  int y = LIST_Y + (row * ROW_H);

  uint16_t bg = isSel ? DARK_GRAY : TFT_BLACK;
  uint16_t fg = isSel ? TFT_WHITE : TFT_LIGHTGREY;
  tft.fillRect(LIST_X, y, LIST_W, ROW_H - 1, bg);
  tft.setTextColor(fg, bg);
  tft.setCursor(LIST_X + 2, y + 4);
  tft.printf("%2d.", (int)globalIdx + 1);
  tft.setCursor(LIST_X + 34, y + 4);

  if (cachedOk[row]) {

    char nameBuf[17];
    memcpy(nameBuf, cachedPage[row].name, 16);
    nameBuf[16] = '\0';
    String nm = String(nameBuf);
    if (nm.length() > 10) nm = nm.substring(0, 10);
    tft.print(nm);

    char fbuf[16];
    snprintf(fbuf, sizeof(fbuf), "%.2f", cachedPage[row].frequency / 1000000.0);
    int tw = tft.textWidth(fbuf, 1);
    tft.setCursor(LIST_X + LIST_W - 4 - tw, y + 4);
    tft.print(fbuf);
  } else {
    tft.print("<?>");
  }
}

static void drawListPage(uint16_t pageStart) {
  ensurePageCache();

  tft.fillRect(LIST_X, LIST_Y, LIST_W, (ITEMS_PER_PAGE * ROW_H), TFT_BLACK);
  for (uint8_t row = 0; row < ITEMS_PER_PAGE; row++) {
    if ((uint16_t)(pageStart + row) >= sdTotalProfiles) break;
    drawRow(pageStart, row);
  }
}

static void drawDetails() {

  tft.fillRect(0, DETAILS_Y, 240, (DETAILS_H + UI_GAP_Y), TFT_BLACK);

  String err;
  if (!selectedValid) loadSelectedFromSd(&err);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, DETAILS_Y);
  if (!selectedValid) {
    tft.print("Read failed: ");
    tft.print(err);
    return;
  }

  tft.print("Name: "); tft.print(selectedProfile.name);
  tft.setCursor(10, DETAILS_Y + 14);
  tft.printf("Freq: %.2f MHz  P:%d", selectedProfile.frequency / 1000000.0, selectedProfile.protocol);
  tft.setCursor(10, DETAILS_Y + 28);
  tft.printf("Val: %lu  Bit:%d", selectedProfile.value, selectedProfile.bitLength);

  tft.setCursor(10, DETAILS_Y + 42);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  if (selectedPath.endsWith("profiles_current.bin")) {
    tft.print("SRC: current");
  } else {
    tft.print("SRC: ");
    int slash = selectedPath.lastIndexOf('/');
    tft.print(slash >= 0 ? selectedPath.substring(slash + 1) : selectedPath);
  }

  if (deleteArmed && (int32_t)(millis() - deleteArmUntilMs) < 0) {

    int hintY = DETAILS_Y + 56;
    if (hintY >= BOT_Y) hintY = BOT_Y - 12;
    tft.setCursor(10, hintY);
    tft.setTextColor(UI_WARN, TFT_BLACK);
    tft.print("Press delete again to confirm");
  }

  drawBottomButtons();
}

static void updateSelectionUI(uint16_t oldIndex, bool forceListRedraw = false) {
  if (sdTotalProfiles == 0) return;
  uint16_t oldPage = pageStartForIndex(oldIndex);
  uint16_t newPage = pageStartForIndex(currentProfileIndex);

  tft.startWrite();
  drawHeaderLine();

  if (forceListRedraw || oldPage != newPage) {
    drawListPage(newPage);
  } else {

    uint8_t oldRow = (uint8_t)(oldIndex - oldPage);
    uint8_t newRow = (uint8_t)(currentProfileIndex - newPage);
    ensurePageCache();

    drawRow(newPage, oldRow);
    drawRow(newPage, newRow);
  }

  drawDetails();
  tft.endWrite();
}

void updateDisplay() {

    tft.startWrite();
    tft.fillRect(0, 40, 240, 280, TFT_BLACK);

    if (sdTotalProfiles == 0) {
        tft.setCursor(10, 35 + yshift);
        tft.setTextColor(TFT_WHITE);
        tft.print("No saved profiles.");
        if (sdLastErr.length()) {
          tft.setCursor(10, 48 + yshift);
          tft.setTextColor(TFT_DARKGREY);
          tft.print(sdLastErr);
        }
        tft.endWrite();
        return;
    }

    drawHeaderLine();
    drawListPage(pageStartForIndex(currentProfileIndex));
    drawDetails();
    tft.endWrite();
}

void transmitProfile(int index) {
    (void)index;
    String err;
    loadSelectedFromSd(&err);
    if (!selectedValid) return;
    Profile profileToSend = selectedProfile;

    subghzRadioInitCommon();
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(profileToSend.frequency / 1000000.0);

    delay(20);
    mySwitch.enableTransmit(SUBGHZ_TX_PIN);
    ELECHOUSE_cc1101.SetTx();

    tft.fillRect(0, 40, 240, 280, TFT_BLACK);
    tft.setCursor(10, 30 + yshift);
    tft.setTextColor(TFT_WHITE);
    tft.print("Sending ");
    tft.print(profileToSend.name);
    tft.print("...");
    tft.setCursor(10, 50 + yshift);
    tft.print("Value: ");
    tft.print(profileToSend.value);

    mySwitch.setProtocol(profileToSend.protocol);
    mySwitch.send(profileToSend.value, profileToSend.bitLength);

    delay(150);
    tft.fillRect(0, 40, 240, 280, TFT_BLACK);
    tft.setCursor(10, 30 + yshift);
    tft.print("Done!");

    ELECHOUSE_cc1101.setSidle();
    delay(150);
    updateDisplay();
}

void loadProfileCount() {
    EEPROM.get(ADDR_PROFILE_COUNT, profileCount);
    if (profileCount > MAX_PROFILES) profileCount = MAX_PROFILES;
    refreshSdIndex(true);
}

void printProfiles() {
    Serial.println("Saved Profiles (EEPROM):");
    EEPROM.get(ADDR_PROFILE_COUNT, profileCount);
    if (profileCount > MAX_PROFILES) profileCount = MAX_PROFILES;
    Serial.print("Total profiles: ");
    Serial.println((int)profileCount);
    for (uint16_t i = 0; i < profileCount; i++) {
      Profile p{};
      EEPROM.get(ADDR_PROFILE_START + (i * PROFILE_SIZE), p);
      p.name[MAX_NAME_LENGTH - 1] = ' ';
      Serial.print("  [");
      Serial.print((int)i);
      Serial.print("] ");
      Serial.print(p.name);
      Serial.print(" @ ");
      Serial.print(p.frequency / 1000000.0);
      Serial.print(" MHz (val=");
      Serial.print((unsigned long)p.value);
      Serial.println(")");
    }
}

void deleteProfile(int index) {
    (void)index;
    if (sdTotalProfiles == 0) return;

    uint32_t now = millis();
    if (!deleteArmed || (int32_t)(now - deleteArmUntilMs) >= 0) {
      deleteArmed = true;
      deleteArmUntilMs = now + 3000;
      updateDisplay();
      return;
    }
    deleteArmed = false;

    if (currentProfileIndex >= sdTotalProfiles) return;

    for (uint16_t i = currentProfileIndex; i + 1 < sdTotalProfiles; i++) {
      Profile nextP{};
      EEPROM.get(ADDR_PROFILE_START + ((i + 1) * PROFILE_SIZE), nextP);
      EEPROM.put(ADDR_PROFILE_START + (i * PROFILE_SIZE), nextP);
    }

    Profile blank{};
    EEPROM.put(ADDR_PROFILE_START + ((sdTotalProfiles - 1) * PROFILE_SIZE), blank);

    profileCount = (sdTotalProfiles > 0) ? (sdTotalProfiles - 1) : 0;
    EEPROM.put(ADDR_PROFILE_COUNT, profileCount);
    EEPROM.commit();

    refreshSdIndex(false);
    if (sdTotalProfiles == 0) currentProfileIndex = 0;
    else if (currentProfileIndex >= sdTotalProfiles) currentProfileIndex = (uint16_t)(sdTotalProfiles - 1);
    selectedValid = false;
    cacheDirty = true;
    updateDisplay();
}

void runUI() {
    #define STATUS_BAR_Y_OFFSET 20
    #define STATUS_BAR_HEIGHT 16
    #define ICON_SIZE 16
    #define ICON_NUM 6

    static int iconX[ICON_NUM] = {90, 130, 170, 210, 50, 10};
    static int iconY = STATUS_BAR_Y_OFFSET;

    static const unsigned char* icons[ICON_NUM] = {
        bitmap_icon_sort_down_minus,
        bitmap_icon_sort_up_plus,
        bitmap_icon_antenna,
        bitmap_icon_recycle,
        bitmap_icon_sdcard,
        bitmap_icon_go_back
    };

    if (!uiDrawn) {
        tft.drawLine(0, 19, 240, 19, TFT_WHITE);
        tft.fillRect(0, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

        for (int i = 0; i < ICON_NUM; i++) {
            if (icons[i] != NULL) {
                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
            }
        }
        tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, STATUS_ICON);
        uiDrawn = true;
    }

    static unsigned long lastAnimationTime = 0;
    static int animationState = 0;
    static int activeIcon = -1;

    if (animationState > 0 && millis() - lastAnimationTime >= 150) {
        if (animationState == 1) {
            tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
            animationState = 2;

            switch (activeIcon) {
                case 0:
                    if (sdTotalProfiles > 0) {
                        uint16_t oldIdx = currentProfileIndex;
                        currentProfileIndex = (uint16_t)((currentProfileIndex + 1) % sdTotalProfiles);
                        selectedValid = false;
                        cacheDirty = true;
                        deleteArmed = false;

                        updateSelectionUI(oldIdx, false);
                    }
                    break;
                case 1:
                    if (sdTotalProfiles > 0) {
                        uint16_t oldIdx = currentProfileIndex;
                        currentProfileIndex = (uint16_t)((currentProfileIndex + sdTotalProfiles - 1) % sdTotalProfiles);
                        selectedValid = false;
                        cacheDirty = true;
                        deleteArmed = false;
                        updateSelectionUI(oldIdx, false);
                    }
                    break;
                case 2:
                    if (sdTotalProfiles > 0) {
                        transmitProfile(currentProfileIndex);
                    }
                    break;
                case 3:
                    if (sdTotalProfiles > 0) {
                        deleteProfile(currentProfileIndex);
                    }
                    break;
                case 4: {
                    refreshSdIndex(true);
                    selectedValid = false;
                    cacheDirty = true;
                    deleteArmed = false;
                    updateDisplay();
                    break;
                }
            }
        } else if (animationState == 2) {
            animationState = 0;
            activeIcon = -1;
        }
        lastAnimationTime = millis();
    }

    static unsigned long lastTouchCheck = 0;
    const unsigned long touchCheckInterval = 50;

    if (millis() - lastTouchCheck >= touchCheckInterval) {
        int x, y;
        if (feature_active && readTouchXY(x, y)) {

            if (y >= BOT_Y && y < (BOT_Y + BOT_H)) {
              if (x >= BOT_TX_X && x < (BOT_TX_X + BOT_BTN_W)) {
                if (sdTotalProfiles > 0) transmitProfile(currentProfileIndex);
              } else if (x >= BOT_DEL_X && x < (BOT_DEL_X + BOT_BTN_W)) {
                if (sdTotalProfiles > 0) deleteProfile(currentProfileIndex);
              }
              lastTouchCheck = millis();
              return;
            }

            if (y >= LIST_Y && y < (LIST_Y + (ITEMS_PER_PAGE * ROW_H)) && x >= LIST_X && x < (LIST_X + LIST_W)) {
              uint8_t row = (uint8_t)((y - LIST_Y) / ROW_H);
              uint16_t oldIdx = currentProfileIndex;
              uint16_t start = pageStartForIndex(currentProfileIndex);
              uint16_t idx = (uint16_t)(start + row);
              if (idx < sdTotalProfiles) {
                currentProfileIndex = idx;
                selectedValid = false;
                cacheDirty = true;
                deleteArmed = false;
                updateSelectionUI(oldIdx, false);
              }
            }
            if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
                for (int i = 0; i < ICON_NUM; i++) {
                    if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
                        if (icons[i] != NULL && animationState == 0) {

                            if (i == 5) {
                                feature_exit_requested = true;
                            } else {

                                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
                                animationState = 1;
                                activeIcon = i;
                                lastAnimationTime = millis();
                            }
                        }
                        break;
                    }
                }
            }
        }
        lastTouchCheck = millis();
    }
}

void saveSetup() {
    setStatusBarSdPollingEnabled(false);
    setStatusBarBatteryPollingEnabled(false);

    EEPROM.begin(EEPROM_SIZE);
    pcf.pinMode(BTN_UP, INPUT_PULLUP);
    pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
    pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
    pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
    pcf.pinMode(BTN_SELECT, INPUT_PULLUP);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    setupTouchscreen();
    drawStatusBar(currentBatteryVoltage, true);
    uiDrawn = false;

    loadProfileCount();
    printProfiles();
    cacheDirty = true;
    deleteArmed = false;
    updateDisplay();
}

void saveLoop() {

    if (feature_active && isButtonPressed(BTN_SELECT)) {
        feature_exit_requested = true;
        return;
    }

    runUI();

    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 200;

    bool prevPressed    = isButtonPressed(BTN_UP);
    bool nextPressed    = isButtonPressed(BTN_DOWN);
    bool txPressed      = isButtonPressed(BTN_RIGHT);
    bool refreshPressed = isButtonPressed(BTN_LEFT);

    if (sdTotalProfiles > 0) {

        if (nextPressed && millis() - lastDebounceTime > debounceDelay) {
            uint16_t oldIdx = currentProfileIndex;
            currentProfileIndex = (uint16_t)((currentProfileIndex + 1) % sdTotalProfiles);
            selectedValid = false;
            updateSelectionUI(oldIdx, false);
            lastDebounceTime = millis();
        }

        if (prevPressed && millis() - lastDebounceTime > debounceDelay) {
            uint16_t oldIdx = currentProfileIndex;
            currentProfileIndex = (uint16_t)((currentProfileIndex + sdTotalProfiles - 1) % sdTotalProfiles);
            selectedValid = false;
            updateSelectionUI(oldIdx, false);
            lastDebounceTime = millis();
        }

        if (txPressed && millis() - lastDebounceTime > debounceDelay) {
            transmitProfile(currentProfileIndex);
            lastDebounceTime = millis();
        }

        if (refreshPressed && millis() - lastDebounceTime > debounceDelay) {
            refreshSdIndex(true);
            selectedValid = false;
            cacheDirty = true;
            deleteArmed = false;
            updateDisplay();
            lastDebounceTime = millis();
        }
    } else {

        tft.setCursor(10, 50 + yshift);
        tft.setTextColor(TFT_WHITE);
        tft.print("No saved profiles.");
    }
}

}

namespace subjammer {

static bool uiDrawn = false;

static unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 64

static constexpr uint8_t JAM_BTN_LEFT  = 4;
static constexpr uint8_t JAM_BTN_RIGHT = 5;
static constexpr uint8_t JAM_BTN_DOWN  = 3;
static constexpr uint8_t JAM_BTN_UP    = 6;

bool jammingRunning = false;
bool continuousMode = true;
bool autoMode = false;
unsigned long lastSweepTime = 0;
const unsigned long sweepInterval = 1000;

static const uint32_t subghz_frequency_list[] = {
    300000000, 303875000, 304250000, 310000000, 315000000, 318000000,
    390000000, 418000000, 433075000, 433420000, 433920000, 434420000,
    434775000, 438900000, 868350000, 915000000, 925000000
};
const int numFrequencies = sizeof(subghz_frequency_list) / sizeof(subghz_frequency_list[0]);
int currentFrequencyIndex = 4;
float targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;

void updateDisplay() {

    int yshift = 20;

    tft.fillRect(0, 40, 240, 80, TFT_BLACK);
    tft.drawLine(0, 79, 235, 79, TFT_WHITE);

    tft.setTextSize(1);
    tft.setCursor(5, 22 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("Freq:");
    tft.setCursor(40, 22 + yshift);
    if (autoMode) {
        tft.setTextColor(STATUS_ICON);
        tft.print("Auto: ");
        tft.setTextColor(TFT_WHITE);
        tft.print(targetFrequency, 1);

        int progress = ::map(currentFrequencyIndex, 0, numFrequencies - 1, 0, 240);
        tft.fillRect(0, 60 + yshift, 240, 4, TFT_BLACK);
        tft.fillRect(0, 60 + yshift, progress, 4, STATUS_ICON);

        if (jammingRunning && millis() % 1000 < 500) {
            tft.fillCircle(220, 22 + yshift, 2, TFT_GREEN);
        }
    } else {
        tft.setTextColor(TFT_WHITE);
        tft.print(targetFrequency, 2);
        tft.print(" MHz");
    }

    tft.setCursor(130, 22 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("Mode:");
    tft.setCursor(165, 22 + yshift);
    tft.setTextColor(continuousMode ? TFT_GREEN : TFT_YELLOW);
    tft.print(continuousMode ? "Cont" : "Noise");

    tft.setCursor(5, 42 + yshift);
    tft.setTextColor(TFT_CYAN);
    tft.print("Status:");
    tft.setCursor(50, 42 + yshift);
    if (jammingRunning) {
        tft.setTextColor(UI_WARN);
        tft.print("Jamming");

    } else {
        tft.setTextColor(TFT_GREEN);
        tft.print("Idle   ");
    }
}

void runUI() {
    #define SCREEN_WIDTH  240
    #define SCREENHEIGHT 320
    #define STATUS_BAR_Y_OFFSET 20
    #define STATUS_BAR_HEIGHT 16
    #define ICON_SIZE 16
    #define ICON_NUM 6

    static int iconX[ICON_NUM] = {50, 90, 130, 170, 210, 10};
    static int iconY = STATUS_BAR_Y_OFFSET;

    static const unsigned char* icons[ICON_NUM] = {
        bitmap_icon_power,
        bitmap_icon_antenna,
        bitmap_icon_random,
        bitmap_icon_sort_down_minus,
        bitmap_icon_sort_up_plus,
        bitmap_icon_go_back
    };

    if (!uiDrawn) {
        tft.drawLine(0, 19, 240, 19, TFT_WHITE);
        tft.fillRect(0, STATUS_BAR_Y_OFFSET, SCREEN_WIDTH, STATUS_BAR_HEIGHT, DARK_GRAY);

        for (int i = 0; i < ICON_NUM; i++) {
            if (icons[i] != NULL) {
                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_WHITE);
            }
        }
        tft.drawLine(0, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, SCREEN_WIDTH, STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT, STATUS_ICON);
        uiDrawn = true;
    }

    static unsigned long lastAnimationTime = 0;
    static int animationState = 0;
    static int activeIcon = -1;

    if (animationState > 0 && millis() - lastAnimationTime >= 150) {
        if (animationState == 1) {
            tft.drawBitmap(iconX[activeIcon], iconY, icons[activeIcon], ICON_SIZE, ICON_SIZE, TFT_WHITE);
            animationState = 2;

            switch (activeIcon) {
                case 0:
                  jammingRunning = !jammingRunning;
                    if (jammingRunning) {
                        Serial.println("Jamming started");
                        ELECHOUSE_cc1101.setMHZ(targetFrequency);
                        ELECHOUSE_cc1101.SetTx();
                    } else {
                        Serial.println("Jamming stopped");
                        ELECHOUSE_cc1101.setSidle();
                        digitalWrite(SUBGHZ_TX_PIN, LOW);
                    }
                    updateDisplay();
                    lastDebounceTime = millis();
                    break;
                case 1:
                 continuousMode = !continuousMode;
                  Serial.print("Jamming mode: ");
                  Serial.println(continuousMode ? "Continuous Carrier" : "Noise");
                  updateDisplay();
                  lastDebounceTime = millis();
                    break;
                case 2:
                  autoMode = !autoMode;
                  Serial.print("Frequency mode: ");
                  Serial.println(autoMode ? "Automatic" : "Manual");
                  if (autoMode) {
                      currentFrequencyIndex = 0;
                      targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
                      ELECHOUSE_cc1101.setMHZ(targetFrequency);
                  }
                  updateDisplay();
                  lastDebounceTime = millis();
                    break;
                case 3:
                  currentFrequencyIndex = (currentFrequencyIndex - 1 + numFrequencies) % numFrequencies;
                  targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
                  ELECHOUSE_cc1101.setMHZ(targetFrequency);
                  Serial.print("Switched to: ");
                  Serial.print(targetFrequency);
                  Serial.println(" MHz");
                  updateDisplay();
                  lastDebounceTime = millis();
                    break;
                 case 4:
                  currentFrequencyIndex = (currentFrequencyIndex + 1) % numFrequencies;
                  targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
                  ELECHOUSE_cc1101.setMHZ(targetFrequency);
                  Serial.print("Switched to: ");
                  Serial.print(targetFrequency);
                  Serial.println(" MHz");
                  updateDisplay();
                  lastDebounceTime = millis();
                    break;
                case 5:
                    feature_exit_requested = true;
                    break;
            }
        } else if (animationState == 2) {
            animationState = 0;
            activeIcon = -1;
        }
        lastAnimationTime = millis();
    }

    static unsigned long lastTouchCheck = 0;
    const unsigned long touchCheckInterval = 50;

    if (millis() - lastTouchCheck >= touchCheckInterval) {
        int x, y;
        if (feature_active && readTouchXY(x, y)) {
            if (y > STATUS_BAR_Y_OFFSET && y < STATUS_BAR_Y_OFFSET + STATUS_BAR_HEIGHT) {
                for (int i = 0; i < ICON_NUM; i++) {
                    if (x > iconX[i] && x < iconX[i] + ICON_SIZE) {
                        if (icons[i] != NULL && animationState == 0) {

                            if (i == 5) {
                                feature_exit_requested = true;
                            } else {

                                tft.drawBitmap(iconX[i], iconY, icons[i], ICON_SIZE, ICON_SIZE, TFT_BLACK);
                                animationState = 1;
                                activeIcon = i;
                                lastAnimationTime = millis();
                            }
                        }
                        break;
                    }
                }
            }
        }
        lastTouchCheck = millis();
    }
}

void subjammerSetup() {
    setStatusBarSdPollingEnabled(false);
    setStatusBarBatteryPollingEnabled(false);

    subghzRadioInitCommon();
    ELECHOUSE_cc1101.setModulation(0);
    ELECHOUSE_cc1101.setRxBW(500.0);
    ELECHOUSE_cc1101.setPA(12);
    ELECHOUSE_cc1101.setMHZ(targetFrequency);
    ELECHOUSE_cc1101.SetTx();

    randomSeed(analogRead(0));

    pcf.pinMode(BTN_LEFT, INPUT_PULLUP);
    pcf.pinMode(BTN_RIGHT, INPUT_PULLUP);
    pcf.pinMode(BTN_DOWN, INPUT_PULLUP);
    pcf.pinMode(BTN_UP, INPUT_PULLUP);
    delay(100);

    tft.fillScreen(TFT_BLACK);

    setupTouchscreen();

    drawStatusBar(currentBatteryVoltage, true);
    updateDisplay();
    uiDrawn = false;
}

void subjammerLoop() {

    if (feature_active && isButtonPressed(BTN_SELECT)) {
        feature_exit_requested = true;
        return;
    }

    runUI();

    int btnLeftState = pcf.digitalRead(JAM_BTN_LEFT);
    int btnRightState = pcf.digitalRead(JAM_BTN_RIGHT);
    int btnUpState = pcf.digitalRead(JAM_BTN_UP);
    int btnDownState = pcf.digitalRead(JAM_BTN_DOWN);

    if (btnUpState == LOW && millis() - lastDebounceTime > debounceDelay) {
        jammingRunning = !jammingRunning;
        if (jammingRunning) {
            Serial.println("Jamming started");
            ELECHOUSE_cc1101.setMHZ(targetFrequency);
            ELECHOUSE_cc1101.SetTx();
        } else {
            Serial.println("Jamming stopped");
            ELECHOUSE_cc1101.setSidle();
            digitalWrite(SUBGHZ_TX_PIN, LOW);
        }
        updateDisplay();
        lastDebounceTime = millis();
    }

    if (btnRightState == LOW && !autoMode && millis() - lastDebounceTime > debounceDelay) {
        currentFrequencyIndex = (currentFrequencyIndex + 1) % numFrequencies;
        targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
        ELECHOUSE_cc1101.setMHZ(targetFrequency);
        Serial.print("Switched to: ");
        Serial.print(targetFrequency);
        Serial.println(" MHz");
        updateDisplay();
        lastDebounceTime = millis();
    }

    if (btnLeftState == LOW && !autoMode && millis() - lastDebounceTime > debounceDelay) {
        continuousMode = !continuousMode;
        Serial.print("Jamming mode: ");
        Serial.println(continuousMode ? "Continuous Carrier" : "Noise");
        updateDisplay();
        lastDebounceTime = millis();
    }

    if (btnDownState == LOW && millis() - lastDebounceTime > debounceDelay) {
        autoMode = !autoMode;
        Serial.print("Frequency mode: ");
        Serial.println(autoMode ? "Automatic" : "Manual");
        if (autoMode) {
            currentFrequencyIndex = 0;
            targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
            ELECHOUSE_cc1101.setMHZ(targetFrequency);
        }
        updateDisplay();
        lastDebounceTime = millis();
    }

    if (jammingRunning) {
        if (autoMode) {
            if (millis() - lastSweepTime >= sweepInterval) {
                currentFrequencyIndex = (currentFrequencyIndex + 1) % numFrequencies;
                targetFrequency = subghz_frequency_list[currentFrequencyIndex] / 1000000.0;
                ELECHOUSE_cc1101.setMHZ(targetFrequency);
                Serial.print("Sweeping: ");
                Serial.print(targetFrequency);
                Serial.println(" MHz");
                updateDisplay();
                lastSweepTime = millis();
            }
        }

        ELECHOUSE_cc1101.SetTx();

        if (continuousMode) {
            ELECHOUSE_cc1101.SpiWriteReg(CC1101_TXFIFO, 0xFF);
            ELECHOUSE_cc1101.SpiStrobe(CC1101_STX);
            digitalWrite(SUBGHZ_TX_PIN, HIGH);
        } else {
            for (int i = 0; i < 10; i++) {
                uint32_t noise = random(16777216);
                ELECHOUSE_cc1101.SpiWriteReg(CC1101_TXFIFO, noise >> 16);
                ELECHOUSE_cc1101.SpiWriteReg(CC1101_TXFIFO, (noise >> 8) & 0xFF);
                ELECHOUSE_cc1101.SpiWriteReg(CC1101_TXFIFO, noise & 0xFF);
                ELECHOUSE_cc1101.SpiStrobe(CC1101_STX);
                delayMicroseconds(50);
              }
          }
      }
  }
}


namespace replayat {

void powerMgrStopSubGhzActivity() {
  #ifdef CC1101_CS
  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, HIGH);
  #endif
  delay(60);
}

}
