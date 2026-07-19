#include "ReadingStats.h"

#include "BookStats.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>
#include <Arduino.h>

#include <cstring>
#include <ctime>

namespace {
constexpr uint8_t STATS_FILE_VERSION = 2;
constexpr char STATS_FILE[] = "/.crosspoint/reading_stats.bin";
}  // namespace

ReadingStats ReadingStats::instance;

void ReadingStats::startSession() {
  sessionStartMillis = millis();
  sessionAccountedSeconds = 0;
  sessionStartYear = 0;
  sessionStartDayOfYear = -1;
  const time_t now = time(nullptr);
  constexpr time_t MIN_VALID_TIME = 1704067200;
  if (now >= MIN_VALID_TIME) {
    struct tm timeinfo = {};
    localtime_r(&now, &timeinfo);
    sessionStartYear = static_cast<int16_t>(timeinfo.tm_year);
    sessionStartDayOfYear = static_cast<int16_t>(timeinfo.tm_yday);
    rollToDay(sessionStartYear, sessionStartDayOfYear);
  }
  sessionActive = true;
}

uint32_t ReadingStats::currentSessionSeconds() const {
  if (!sessionActive) return 0;
  // Unsigned subtraction remains correct across the millis() wraparound.
  return sessionAccountedSeconds + (millis() - sessionStartMillis) / 1000;
}

uint32_t ReadingStats::currentDaySessionSeconds() const {
  if (!sessionActive) return 0;
  return (millis() - sessionStartMillis) / 1000;
}

void ReadingStats::rollToDay(int16_t year, int16_t dayOfYear) {
  if (dayOfYear == lastReadDayOfYear && year == lastReadYear) return;
  if (lastReadDayOfYear >= 0 &&
      (year < lastReadYear || (year == lastReadYear && dayOfYear < lastReadDayOfYear))) {
    return;
  }

  bool isConsecutive = false;
  if (lastReadDayOfYear >= 0) {
    if (year == lastReadYear && dayOfYear == lastReadDayOfYear + 1) {
      isConsecutive = true;
    } else if (year == lastReadYear + 1 && dayOfYear == 0) {
      isConsecutive = (lastReadDayOfYear == 364 || lastReadDayOfYear == 365);
    }
  }
  currentStreak = isConsecutive ? static_cast<uint16_t>(currentStreak + 1) : 1;
  if (currentStreak > longestStreak) longestStreak = currentStreak;

  todayReadSeconds = 0;
  lastReadYear = year;
  lastReadDayOfYear = dayOfYear;
}

bool ReadingStats::checkpointForClockChange(uint32_t& closedDaySeconds) {
  closedDaySeconds = 0;
  if (!sessionActive || sessionStartDayOfYear < 0) return false;

  constexpr time_t MIN_VALID_TIME = 1704067200;
  const time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) return false;
  struct tm timeinfo = {};
  localtime_r(&now, &timeinfo);
  const int16_t year = static_cast<int16_t>(timeinfo.tm_year);
  const int16_t day = static_cast<int16_t>(timeinfo.tm_yday);
  if (year == sessionStartYear && day == sessionStartDayOfYear) return false;
  // A backward correction must not manufacture a second day transition.
  if (year < sessionStartYear || (year == sessionStartYear && day < sessionStartDayOfYear)) return false;

  const uint32_t segmentSeconds = currentDaySessionSeconds();
  todayReadSeconds += segmentSeconds;
  totalReadSeconds += segmentSeconds;
  sessionAccountedSeconds += segmentSeconds;
  sessionStartMillis = millis();
  closedDaySeconds = todayReadSeconds;

  rollToDay(year, day);
  sessionStartYear = year;
  sessionStartDayOfYear = day;
  return true;
}

void ReadingStats::endSession(const char* title, uint8_t progress, const char* bookPath) {
  if (!sessionActive) return;

  // Check for day rollover; reset today's counter and update streak.
  // Only do this when we have a valid wall-clock time (NTP synced, >= 2024).
  constexpr time_t MIN_VALID_TIME = 1704067200;  // 2024-01-01 00:00:00 UTC
  time_t now = time(nullptr);
  struct tm timeinfo = {};
  localtime_r(&now, &timeinfo);
  const int16_t curYear = static_cast<int16_t>(timeinfo.tm_year);
  const int16_t curDay  = static_cast<int16_t>(timeinfo.tm_yday);
  if (now >= MIN_VALID_TIME && (curDay != lastReadDayOfYear || curYear != lastReadYear)) {
    rollToDay(curYear, curDay);
  }

  // Session duration must use monotonic time: NTP and manual clock corrections can
  // change time(nullptr) while the reader is open. Sessions end before deep sleep.
  constexpr uint32_t MAX_SESSION_SECS = 86400;
  const uint32_t sessionSecs = currentSessionSeconds();
  const uint32_t elapsedSecs = sessionSecs < MAX_SESSION_SECS ? currentDaySessionSeconds() : 0;

  todayReadSeconds += elapsedSecs;
  totalReadSeconds += elapsedSecs;
  totalSessions++;

  // Track book completion
  uint8_t prevProgress = lastBookProgress;
  if (title && title[0] != '\0') {
    strncpy(lastBookTitle, title, sizeof(lastBookTitle) - 1);
    lastBookTitle[sizeof(lastBookTitle) - 1] = '\0';
  }
  lastBookProgress = progress;
  if (progress >= 100 && prevProgress < 100) {
    booksFinished++;
  }

  sessionActive = false;
  sessionAccountedSeconds = 0;
  saveToFile();

  // Update per-book stats if path was provided
  if (bookPath && bookPath[0] != '\0') {
    BOOK_STATS.updateBook(bookPath, title, sessionSecs < MAX_SESSION_SECS ? sessionSecs : 0, progress);
  }
}

bool ReadingStats::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  FsFile file;
  if (!Storage.openFileForWrite("RST", STATS_FILE, file)) {
    LOG_ERR("RST", "Failed to open reading_stats.bin for write");
    return false;
  }
  const uint8_t version = STATS_FILE_VERSION;
  serialization::writePod(file, version);
  serialization::writePod(file, totalReadSeconds);
  serialization::writePod(file, todayReadSeconds);
  serialization::writePod(file, lastReadYear);
  serialization::writePod(file, lastReadDayOfYear);
  serialization::writePod(file, lastBookProgress);
  file.write(reinterpret_cast<const uint8_t*>(lastBookTitle), sizeof(lastBookTitle));
  // v2 extended fields
  serialization::writePod(file, totalSessions);
  serialization::writePod(file, booksFinished);
  serialization::writePod(file, currentStreak);
  serialization::writePod(file, longestStreak);
  file.close();
  return true;
}

bool ReadingStats::loadFromFile() {
  FsFile file;
  if (!Storage.openFileForRead("RST", STATS_FILE, file)) {
    return false;
  }
  uint8_t version;
  serialization::readPod(file, version);
  if (version != 1 && version != STATS_FILE_VERSION) {
    LOG_ERR("RST", "Unknown reading_stats.bin version %u", version);
    file.close();
    return false;
  }
  serialization::readPod(file, totalReadSeconds);
  serialization::readPod(file, todayReadSeconds);
  serialization::readPod(file, lastReadYear);
  serialization::readPod(file, lastReadDayOfYear);
  serialization::readPod(file, lastBookProgress);
  file.read(reinterpret_cast<uint8_t*>(lastBookTitle), sizeof(lastBookTitle));
  lastBookTitle[sizeof(lastBookTitle) - 1] = '\0';
  // v2 extended fields (defaults to 0 if upgrading from v1)
  if (version >= 2) {
    serialization::readPod(file, totalSessions);
    serialization::readPod(file, booksFinished);
    serialization::readPod(file, currentStreak);
    serialization::readPod(file, longestStreak);
  }
  file.close();
  // Re-save as v2 if loaded v1
  if (version < STATS_FILE_VERSION) saveToFile();

  // Load per-book stats alongside global stats
  BOOK_STATS.loadFromFile();
  return true;
}
