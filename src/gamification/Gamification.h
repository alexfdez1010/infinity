#pragma once
#include <I18n.h>

#include <cstddef>
#include <cstdint>

// Reading gamification layer: daily goal, streak with freeze tokens,
// achievement badges, weekly history and personal records.
// Consumes ReadingStats/BookStats; owns no reading tracking of its own.
// State persisted as JSON on SD card.

// Achievement ids double as bit positions in unlockedMask (max 32).
enum class AchievementId : uint8_t {
  FirstBook,
  Books5,
  Books25,
  Streak7,
  Streak30,
  Streak100,
  Hours10,
  Hours50,
  Hours100,
  Hours500,
  DeepSession,   // 60 min in a single session
  MarathonDay,   // 3 h in a single day
  GoalWeek,      // daily goal met 7 days in a row
  Goal30,        // daily goal met 30 days total
  Sessions100,
  COUNT
};

struct AchievementDef {
  StrId name;
  StrId desc;
};

// Static definition table indexed by AchievementId
const AchievementDef& achievementDef(uint8_t idx);

class GamificationManager {
 public:
  GamificationManager(const GamificationManager&) = delete;
  GamificationManager& operator=(const GamificationManager&) = delete;

  static GamificationManager& getInstance();

  // --- Persisted state ---
  uint16_t dailyGoalMinutes = 20;  // adjustable 5..240
  uint16_t streak = 0;             // reading-day streak (freeze tokens can bridge gaps)
  uint16_t longestStreak = 0;
  uint8_t  freezeTokens = 0;       // earned every 7 streak days, max 3; auto-consumed on 1-2 day gaps
  uint16_t goalStreak = 0;         // consecutive closed days with goal met
  uint16_t goalDaysTotal = 0;      // lifetime days with goal met
  uint32_t unlockedMask = 0;       // achievement bits
  uint32_t bestSessionSeconds = 0;
  uint32_t bestDaySeconds = 0;
  uint16_t weekMinutes[7] = {};    // minutes read per day, [0]=today .. [6]=6 days ago

  bool loadFromFile();
  bool saveToFile() const;

  // Call after READ_STATS.startSession(): rolls the day so live polls during the
  // first session of a new day see fresh goal/streak state.
  void onSessionStart() { rollDay(); }

  // Call after READ_STATS.endSession(): rolls the day, updates ring/streak/records,
  // evaluates achievements and persists.
  void onSessionEnd();

  // Cheap live check while reading (call on page turns). Fills `toast` and returns
  // true when the daily goal is first met today or a new achievement unlocks.
  bool pollReader(char* toast, size_t toastLen);

  bool isUnlocked(AchievementId id) const {
    return unlockedMask & (1UL << static_cast<uint8_t>(id));
  }
  uint8_t unlockedCount() const;

  // Minutes read today including the live session (safe outside the reader too)
  uint32_t liveTodaySeconds() const;
  bool goalMetToday() const { return liveTodaySeconds() >= dailyGoalMinutes * 60UL; }

  void adjustGoal(int deltaMinutes);

 private:
  GamificationManager() = default;
  static GamificationManager instance;

  // Persisted day tracking (mirrors ReadingStats wall-clock convention)
  int16_t lastYear = 0;
  int16_t lastDayOfYear = -1;
  bool goalCelebrated = false;         // goal-met toast given for today
  uint32_t lastKnownTotalSeconds = 0;  // for per-session delta (records)

  // If the wall-clock day changed since the last recorded reading day, close the
  // old day (goal streak bookkeeping), shift the week ring and update the streak.
  void rollDay();

  // Set newly earned achievement bits; returns id of the first new unlock or COUNT.
  AchievementId evaluateAchievements();

  static constexpr char STATE_PATH[] = "/.crosspoint/gamification.json";
};

#define GAMIFY GamificationManager::getInstance()
