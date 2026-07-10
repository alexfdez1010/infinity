#pragma once
#include <cstdint>
#include <string>

class CrossPointState {
  // Static instance
  static CrossPointState instance;

 public:
  static constexpr uint8_t SLEEP_RECENT_COUNT = 16;

  std::string openEpubPath;
  uint16_t recentSleepImages[SLEEP_RECENT_COUNT] = {};  // circular buffer of recent wallpaper indices
  uint8_t recentSleepPos = 0;                           // next write slot
  uint8_t recentSleepFill = 0;                          // valid entries (0..SLEEP_RECENT_COUNT)
  uint8_t readerActivityLoadCount = 0;
  bool lastSleepFromReader = false;

  // Returns true if idx was shown within the last checkCount picks.
  // Walks backwards from the most recently written slot.
  bool isRecentSleep(uint16_t idx, uint8_t checkCount) const;

  void pushRecentSleep(uint16_t idx);
  ~CrossPointState() = default;

  // Get singleton instance
  static CrossPointState& getInstance() { return instance; }

  bool saveToFile() const;

  bool loadFromFile();

 private:
  bool loadFromBinaryFile();
};

// Helper macro to access settings
#define APP_STATE CrossPointState::getInstance()

// Clear the boot crash guard once a reader has rendered a stable, responsive
// frame. The guard (readerActivityLoadCount) is bumped before goToReader() on
// wake so a reader that crashes during load boots to Home next time instead of
// re-crashing. It must be cleared after a genuine render — NOT only in onExit(),
// which never runs on deep sleep, or a wake-resume then re-sleep strands the
// guard and forces the next wake to Home. Guarded write avoids per-page SD hits.
inline void clearBootCrashGuard() {
  if (APP_STATE.readerActivityLoadCount != 0) {
    APP_STATE.readerActivityLoadCount = 0;
    APP_STATE.saveToFile();
  }
}
