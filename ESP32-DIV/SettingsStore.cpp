#include <ArduinoJson.h>
#include <SD.h>
#include "SettingsStore.h"
#include <Preferences.h>
#include "SDCardManager.h"


static AppSettings g_settings;
AppSettings& settings() { return g_settings; }


static bool ensureDir(const char* dirPath) {
  SDCardManager::invalidate();
  return SDCardManager::ensureDir(dirPath);
}

bool settingsLoad() {
  Preferences prefs;
  if (!prefs.begin("settings", true)) return false;

  auto& s = g_settings;
  s.brightness      = (uint8_t)prefs.getUChar("brightness", s.brightness);
  s.theme           = (Theme)(uint8_t)prefs.getUChar("theme", (uint8_t)s.theme);
  s.neopixelEnabled = prefs.getBool("neopixel", s.neopixelEnabled);
  s.autoWifiScan    = prefs.getBool("autoWifi", s.autoWifiScan);
  s.autoBleScan     = prefs.getBool("autoBle", s.autoBleScan);
  s.touchXMin       = (uint16_t)prefs.getUInt("txmin", s.touchXMin);
  s.touchXMax       = (uint16_t)prefs.getUInt("txmax", s.touchXMax);
  s.touchYMin       = (uint16_t)prefs.getUInt("tymin", s.touchYMin);
  s.touchYMax       = (uint16_t)prefs.getUInt("tymax", s.touchYMax);

  prefs.end();
  return true;
}

bool settingsSave() {
  Preferences prefs;
  if (!prefs.begin("settings", false)) return false;

  auto& s = g_settings;
  bool ok = true;
  ok &= prefs.putUChar("brightness", s.brightness) > 0;
  ok &= prefs.putUChar("theme", (uint8_t)s.theme) > 0;
  ok &= prefs.putBool("neopixel", s.neopixelEnabled);
  ok &= prefs.putBool("autoWifi", s.autoWifiScan);
  ok &= prefs.putBool("autoBle", s.autoBleScan);
  ok &= prefs.putUInt("txmin", (uint32_t)s.touchXMin) > 0;
  ok &= prefs.putUInt("txmax", (uint32_t)s.touchXMax) > 0;
  ok &= prefs.putUInt("tymin", (uint32_t)s.touchYMin) > 0;
  ok &= prefs.putUInt("tymax", (uint32_t)s.touchYMax) > 0;

  prefs.end();
  return ok;
}
