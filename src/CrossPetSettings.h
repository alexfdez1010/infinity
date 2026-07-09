#pragma once
#include <cstdint>

// CrossPet-specific settings stored separately from CrossPointSettings.
// This allows CrossPet firmware to have its own settings file without
// conflicting with CrossPoint reader settings when users switch firmware.
class CrossPetSettings {
 public:
  // Prevent copy/assign (singleton)
  CrossPetSettings(const CrossPetSettings&) = delete;
  CrossPetSettings& operator=(const CrossPetSettings&) = delete;

  static CrossPetSettings& getInstance();

  // Persist to /.crosspoint/crosspet.json
  bool saveToFile() const;
  // Load from /.crosspoint/crosspet.json; migrates from settings.json if absent
  bool loadFromFile();

  // Home screen layout
  uint8_t homeFocusMode = 0;  // 1=show only current book (no recent covers/stats)

  // Beta: apply custom SD-card font to UI text (filenames, menus) as well.
  // Off by default — enabling may slow UI rendering for scripts not in glyph cache.
  uint8_t systemWideCustomFont = 0;

  // Per-app visibility — controls both Tools menu and home screen widgets (1=show, 0=hide)
  uint8_t appClock = 1;
  // Reading stats & goals are always active (no toggle).
  uint8_t appReadingStats = 1;
  uint8_t appReadingGoals = 1;  // Gamification: goals, streaks, achievements
  uint8_t appGames = 1;  // Master toggle for all games (2048, Sliding Puzzle, Apagón, Senku)

 private:
  CrossPetSettings() = default;
  static CrossPetSettings instance;

  static constexpr char SETTINGS_PATH[] = "/.crosspoint/crosspet.json";
};

// Helper macro to access CrossPet settings
#define CROSSPET_SETTINGS CrossPetSettings::getInstance()
