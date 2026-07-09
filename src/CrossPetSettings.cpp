#include "CrossPetSettings.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

// Initialize static singleton instance
CrossPetSettings CrossPetSettings::instance;

CrossPetSettings& CrossPetSettings::getInstance() {
  return instance;
}

bool CrossPetSettings::saveToFile() const {
  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  doc["appClock"] = appClock;
  doc["appReadingStats"] = appReadingStats;
  doc["appReadingGoals"] = appReadingGoals;
  doc["appGames"] = appGames;

  String json;
  serializeJson(doc, json);
  bool ok = Storage.writeFile(SETTINGS_PATH, json);
  if (ok) {
    LOG_DBG("CPS", "CrossPet settings saved");
  } else {
    LOG_ERR("CPS", "Failed to save CrossPet settings");
  }
  return ok;
}

bool CrossPetSettings::loadFromFile() {
  // Primary: load from crosspet.json
  if (Storage.exists(SETTINGS_PATH)) {
    String json = Storage.readFile(SETTINGS_PATH);
    if (!json.isEmpty()) {
      JsonDocument doc;
      auto error = deserializeJson(doc, json);
      if (error) {
        LOG_ERR("CPS", "CrossPet settings JSON parse error: %s", error.c_str());
        return false;
      }
      // homeFocusMode ("Modo concentración") is removed — always disabled.
      homeFocusMode = 0;

      // ArduinoJson's | operator treats 0/false as "absent", so we must use containsKey
      // to distinguish "user saved 0 (off)" from "key missing (default on)".
      if (doc.containsKey("appClock"))
        appClock = doc["appClock"].as<uint8_t>();
      else if (doc.containsKey("homeShowClock"))
        appClock = doc["homeShowClock"].as<uint8_t>();
      // else: keep struct default (1)

      // Reading stats & goals are always active.
      appReadingStats = 1;
      appReadingGoals = 1;
      appGames = doc["appGames"] | (uint8_t)1;
      LOG_DBG("CPS", "CrossPet settings loaded from file");
      return true;
    }
  }

  // Migration: if crosspet.json absent, try to read values from settings.json
  constexpr char LEGACY_PATH[] = "/.crosspoint/settings.json";
  if (Storage.exists(LEGACY_PATH)) {
    String json = Storage.readFile(LEGACY_PATH);
    if (!json.isEmpty()) {
      JsonDocument doc;
      auto error = deserializeJson(doc, json);
      if (!error) {
        // Migrate legacy homeShow* to app* toggles
        appClock = doc["homeShowClock"] | (uint8_t)1;
        LOG_DBG("CPS", "CrossPet settings migrated from settings.json");
        // Persist the migrated values to crosspet.json
        saveToFile();
        return true;
      }
    }
  }

  // No existing settings — defaults are already set by struct initializers
  LOG_DBG("CPS", "CrossPet settings: no file found, using defaults");
  return false;
}
