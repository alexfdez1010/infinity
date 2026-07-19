#pragma once
#include <cstdint>

// Tracks reading session time and book progress for the Reading Stats sleep screen.
// Stats are accumulated each time the reader activity exits (including on sleep entry).
class ReadingStats {
  static ReadingStats instance;
  uint32_t sessionStartMillis = 0;  // Monotonic start; unaffected by NTP or manual clock changes
  uint32_t sessionAccountedSeconds = 0;
  int16_t sessionStartYear = 0;
  int16_t sessionStartDayOfYear = -1;
  bool sessionActive = false;

  void rollToDay(int16_t year, int16_t dayOfYear);

 public:
  uint32_t totalReadSeconds = 0;    // All-time cumulative reading time
  uint32_t todayReadSeconds = 0;    // Current-day reading time (resets at midnight)
  int16_t  lastReadYear = 0;        // tm_year when last session was recorded
  int16_t  lastReadDayOfYear = -1;  // tm_yday when last session was recorded
  char     lastBookTitle[64] = {};  // Title of the last book read
  uint8_t  lastBookProgress = 0;    // 0-100% progress in last book

  // Extended stats (v2)
  uint16_t totalSessions = 0;       // Total reading sessions
  uint16_t booksFinished = 0;       // Books that reached 100%
  uint16_t currentStreak = 0;       // Consecutive days with reading
  uint16_t longestStreak = 0;       // Best streak ever

  static ReadingStats& getInstance() { return instance; }

  // Called when entering the reader activity; marks session start time.
  void startSession();

  // Called when exiting the reader activity; accumulates elapsed time and saves.
  // bookPath is optional — if provided, per-book stats are updated via BookStats.
  void endSession(const char* title, uint8_t progress, const char* bookPath = nullptr);

  // Seconds elapsed in the active session (0 when idle).
  uint32_t currentSessionSeconds() const;

  // Active-session seconds belonging to the current wall-clock day.
  uint32_t currentDaySessionSeconds() const;

  // When NTP moves an active session into another day, close its pre-sync
  // segment on the old day and continue timing from the corrected day.
  bool checkpointForClockChange(uint32_t& closedDaySeconds);

  bool saveToFile() const;
  bool loadFromFile();
};

#define READ_STATS ReadingStats::getInstance()
