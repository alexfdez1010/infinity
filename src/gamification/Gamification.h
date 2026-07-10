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
  Books50,
  Streak365,
  Goal100,       // daily goal met 100 days total
  EarlyBird,     // daily goal met before 9 am
  NightOwl,      // reading past 11 pm
  Quests25,      // 25 daily quests completed
  COUNT
};

// Daily quest pool: one quest per calendar day, picked deterministically from
// the date. All quests are achievable at any hour of the day.
enum class QuestId : uint8_t {
  FocusSession,   // one session of 20+ min
  Overachieve,    // reach 150% of the daily goal
  TwoSessions,    // two separate sessions today
  Chapter,        // finish a chapter
  Chapters2,      // finish two chapters
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
  uint16_t questsCompletedTotal = 0;  // lifetime completed daily quests
  bool questDoneToday = false;
  uint8_t todaySessions = 0;       // reading sessions started today
  uint8_t chaptersToday = 0;       // chapters finished today

  bool loadFromFile();
  bool saveToFile() const;

  // Call after READ_STATS.startSession(): rolls the day so live polls during the
  // first session of a new day see fresh goal/streak state.
  void onSessionStart() {
    rollDay();
    if (todaySessions < 255) todaySessions++;
  }

  // Call when the reader crosses a chapter boundary going forward (quest input).
  void onChapterFinished() {
    if (chaptersToday < 255) chaptersToday++;
  }

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

  // Today's quest, picked deterministically from the calendar date.
  // Returns QuestId::COUNT when the wall clock is not valid yet.
  QuestId todaysQuest() const;
  // i18n description key for a quest
  static StrId questDesc(QuestId id);
  // Days with goal met in the current 7-day ring (today included when met)
  uint8_t weekGoalDays() const;

 private:
  GamificationManager() = default;
  static GamificationManager instance;

  // Persisted day tracking (mirrors ReadingStats wall-clock convention)
  int16_t lastYear = 0;
  int16_t lastDayOfYear = -1;
  bool goalCelebrated = false;         // goal-met toast given for today
  uint32_t lastKnownTotalSeconds = 0;  // for per-session delta (records)
  bool nudgeGiven = false;             // goal-gradient "almost there" toast shown today (not persisted)
  bool surprisePending = false;        // surprise freeze-token toast queued (not persisted)

  // If the wall-clock day changed since the last recorded reading day, close the
  // old day (goal streak bookkeeping), shift the week ring and update the streak.
  void rollDay();

  // Set newly earned achievement bits; returns id of the first new unlock or COUNT.
  AchievementId evaluateAchievements();

  // True when today's quest condition currently holds
  bool questConditionMet() const;
  // Variable-ratio reward: deterministic pseudo-random chance of a bonus freeze
  // token whenever a goal/quest is completed. Queues a toast when granted.
  void rollSurpriseReward();

  static constexpr char STATE_PATH[] = "/.crosspoint/gamification.json";
};

#define GAMIFY GamificationManager::getInstance()
