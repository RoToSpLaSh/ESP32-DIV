#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "shared.h"

namespace SDCardManager {
  SPIClass& spi();
  bool begin();
  bool ensureMounted();
  bool refreshPresence();
  bool isPresent();
  bool ensureDir(const char* dirPath);
  void invalidate();
}

#endif
