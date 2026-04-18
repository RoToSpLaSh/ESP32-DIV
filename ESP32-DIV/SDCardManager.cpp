#include "SDCardManager.h"

namespace SDCardManager {
  static SPIClass* s_spi = nullptr;
  static bool s_mounted = false;
  static bool s_present = false;
  static uint32_t s_lastAttemptMs = 0;

  SPIClass& spi() {
    if (!s_spi) s_spi = new SPIClass(FSPI);
    return *s_spi;
  }

  bool ensureMounted() {
    if (s_mounted) {
      if (SD.exists("/")) {
        s_present = true;
        return true;
      }
      s_mounted = false;
    }

    const uint32_t now = millis();
    if (!s_present && (now - s_lastAttemptMs) < 250) return false;
    s_lastAttemptMs = now;

    SPIClass& bus = spi();
    bus.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

    s_mounted = SD.begin(SD_CS, bus);
    s_present = s_mounted;
    return s_mounted;
  }

  bool begin() { return ensureMounted(); }

  bool refreshPresence() {
    if (s_mounted && SD.exists("/")) {
      s_present = true;
      return true;
    }
    return ensureMounted();
  }

  bool isPresent() {
    return s_present;
  }

  bool ensureDir(const char* dirPath) {
    if (!ensureMounted()) return false;
    if (SD.exists(dirPath)) return true;
    if (SD.mkdir(dirPath)) return true;
    if (dirPath && dirPath[0] == '/') return SD.mkdir(dirPath + 1);
    return false;
  }

  void invalidate() {
    s_mounted = false;
    s_present = false;
  }
}
